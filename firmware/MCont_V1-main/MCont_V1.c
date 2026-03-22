#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "pwm.pio.h"
#include "gate_drive.pio.h"
#include "pico/time.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/io_bank0.h"

// ====================================================================
//
//     ↓ telemetry code ↓
//    (thank you daniel h sorokin my goat)
//
// ====================================================================

// SPI configuration for telemetry
#define SPI_PORT spi0
#define PIN_RX   8
#define PIN_CS   9
#define PIN_SCK  6
#define PIN_TX   7

#define FLAG_BYTE 0x7E
#define TELEMETRY_PAYLOAD_LEN (4u + 4u + 4u)   // ts_us + motor_rpm + throttle
#define TELEMETRY_FRAME_MAX_BYTES ( \
    ( ( ( ((TELEMETRY_PAYLOAD_LEN + 4u) * 8u * 6u + 4u) / 5u ) + 16u ) + 7u ) / 8u \
)

static int spi_tx_dma_chan = -1;
static uint8_t payload_buf[TELEMETRY_PAYLOAD_LEN];
static uint8_t frame_buf[TELEMETRY_FRAME_MAX_BYTES];

typedef struct __attribute__((packed)) {
    uint32_t ts_us;
    float motor_rpm;
    float throttle;
} telemetry_t;

volatile telemetry_t latest_telem = {0};

static inline void set_gpio_hi_z(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
}

static inline void pack_u32_le(uint8_t *dst, uint32_t x) {
    dst[0] = (uint8_t)(x & 0xFF);
    dst[1] = (uint8_t)((x >> 8) & 0xFF);
    dst[2] = (uint8_t)((x >> 16) & 0xFF);
    dst[3] = (uint8_t)((x >> 24) & 0xFF);
}

static inline void pack_f32_le(uint8_t *dst, float f) {
    memcpy(dst, &f, 4);
}

static uint32_t crc32_ieee(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

static inline bool push_bit(uint8_t *out, uint32_t *bitpos, uint8_t bit) {
    uint32_t byte_i = (*bitpos) >> 3;
    uint32_t bit_i  = (*bitpos) & 7;
    if (byte_i >= TELEMETRY_FRAME_MAX_BYTES) return false;
    if (bit) out[byte_i] |= (uint8_t)(1u << (7 - bit_i));
    (*bitpos)++;
    return true;
}

static bool bit_stuff(const uint8_t *in, size_t len, uint8_t *out, uint32_t *bitpos) {
    int ones = 0;

    for (size_t i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            uint8_t bit = (in[i] >> b) & 1u;
            if (!push_bit(out, bitpos, bit)) return false;

            if (bit) ones++;
            else     ones = 0;

            if (ones == 5) {
                if (!push_bit(out, bitpos, 0)) return false;
                ones = 0;
            }
        }
    }
    return true;
}

static void build_payload(uint8_t *payload) {
    telemetry_t snap = latest_telem;
    pack_u32_le(&payload[0], snap.ts_us);
    pack_f32_le(&payload[4], snap.motor_rpm / 5.0f); // divide by 5 for gearbox
    pack_f32_le(&payload[8], snap.throttle);
}

// START + STUFF(payload|crc) + END
static uint32_t build_frame(uint8_t *out, const uint8_t *payload) {
    uint8_t tmp[TELEMETRY_PAYLOAD_LEN + 4u];
    memcpy(tmp, payload, TELEMETRY_PAYLOAD_LEN);

    uint32_t crc = crc32_ieee(payload, TELEMETRY_PAYLOAD_LEN);
    pack_u32_le(&tmp[TELEMETRY_PAYLOAD_LEN], crc);

    memset(out, 0, TELEMETRY_FRAME_MAX_BYTES);
    out[0] = FLAG_BYTE;

    uint32_t bitpos = 8; // after first flag byte
    if (!bit_stuff(tmp, sizeof(tmp), out, &bitpos)) return 0;

    uint32_t bytes = (bitpos + 7u) >> 3;
    if (bytes + 1u > TELEMETRY_FRAME_MAX_BYTES) return 0;

    out[bytes] = FLAG_BYTE;
    return bytes + 1u;
}

static void configure_spi_dma(void) {
    spi_tx_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(spi_tx_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_SPI0_TX);

    dma_channel_configure(
        spi_tx_dma_chan,
        &c,
        &spi_get_hw(SPI_PORT)->dr,
        frame_buf,
        0,
        false
    );
}

static void answer_spi(void) {
    if (spi_tx_dma_chan >= 0) dma_channel_abort(spi_tx_dma_chan);

    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);

    spi_get_hw(SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
    while (spi_is_readable(SPI_PORT))
        (void)spi_get_hw(SPI_PORT)->dr;

    build_payload(payload_buf);
    uint32_t frame_len = build_frame(frame_buf, payload_buf);
    if (frame_len == 0) {
        set_gpio_hi_z(PIN_TX);
        return;
    }

    dma_channel_set_read_addr(spi_tx_dma_chan, frame_buf, false);
    dma_channel_set_trans_count(spi_tx_dma_chan, frame_len, true);
}

static void configure_spi_slave(void) {
    spi_init(SPI_PORT, 1000 * 1000);
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_1, false);

    gpio_set_function(PIN_RX, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);
    set_gpio_hi_z(PIN_TX);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_IN);
    gpio_disable_pulls(PIN_CS);
}

static void spi_irq_handler(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        if (spi_tx_dma_chan >= 0) { dma_channel_abort(spi_tx_dma_chan); }
        set_gpio_hi_z(PIN_TX);
    } else if (events & GPIO_IRQ_EDGE_RISE) {
        answer_spi();
    }
}

void core1_entry() {    
    configure_spi_slave();

    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled_with_callback(
        PIN_CS, 
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, 
        true, 
        &spi_irq_handler
    );

    configure_spi_dma();
    
    while (true) {
        tight_loop_contents();
    }
}

// ====================================================================
//
//     ↓ motor control code ↓
//    (keeping all the telemetry-related code above this line, 
//     except initialization in main() and updating the telemetry_t struct)
//
// ====================================================================

#define OUT_PINS 10
#define PWM_PIN 16
#define LED 17
#define THROTTLE_ADC 26

// the rotor position as defined by hall input values. 
// note that this value does not change incrementally as the motor moves!
volatile int state;

#define TAU 1e6 // time constant in us for low-pass filter
const uint32_t timer_us = 100 * 1000; // 100ms update velocity without hall trigger

// controls, velocity feedback, and volts per hertz values
volatile int dir = 1; // Car dir = 1, dyno dir = 1
volatile uint32_t prev_control = 0b000000;
volatile float motor_rpm = 0.0f;
const float MAX_VOLTAGE_AT_STALL = 6.0f;
// const float RATED_MOTOR_VOLTAGE = 48.0f;
// const float RATED_MOTOR_RPM = 3000.0f;
const float RATED_MOTOR_VOLTAGE = 31.0f;
const float RATED_MOTOR_RPM = 31.0f * (3000.0f / 48.0f);
const float MAX_DUTY_AT_STALL = MAX_VOLTAGE_AT_STALL / RATED_MOTOR_VOLTAGE;

#define MIN_RPM 25.0f
#define WRAPVAL 255

// if the motor is stationary, then the interrupt needs to be called
// periodically to avoid the interrupt from never being called
// and the motor_rpm from never being updated
struct repeating_timer timer;

// time of last irq
volatile uint32_t irq_prev_time = 0;

// commutation table
const uint8_t shift[2][6][3] = {
    { // clockwise
        {4,3,2},
        {2,1,0},
        {4,1,0},
        {0,5,4},
        {0,3,2},
        {2,5,4},
    },
    { // counterclockwise
        {2,5,4},
        {0,3,2},
        {0,5,4},
        {4,1,0},
        {2,1,0},
        {4,3,2},
    }
};

const uint8_t next[2][6] = { 
    { 4, 2, 0, 5, 3, 1 }, // clockwise
    { 2, 5, 1, 4, 0, 3 }  // counterclockwise
};

// PWM PIO state machine
PIO pwm_pio = pio0;
int pwm_sm = 0;

// GATE DRIVE PIO state machine
PIO gd_pio = pio1;
int gd_sm = 1;

// hall inputs
#define NUM_INPUTS 3
const uint input_pins[NUM_INPUTS] = {20, 19, 18};

#define abs(a) ((a>0) ? a:-a)

float constrainf(float x, float min, float max) {
    if (x < min) { return min; }
    if (x > max) { return max; }
    return x;
}

int adc_deadzone(int adc_value)
{
    const int DEADZONE = 100; // adc units

    if (adc_value < DEADZONE)
        return 0;
    if (adc_value > 4095 - DEADZONE)
        return 4095;
    adc_value = (adc_value - DEADZONE) * 4095 / (4095 - DEADZONE);
    return adc_value;
}

// Write `period` to the input shift register
void pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
    pio->txf[sm] = level;
}

// Update values to drive the phases.
void update_control() {
    int pwm = gpio_get(PWM_PIN); 
    uint32_t control = 0b000000 | (1 << shift[dir][state][0]) | (pwm << shift[dir][state][1]) | (!pwm << shift[dir][state][2]); 

    // Only use deadtime for switching phase; i.e. the grounded phase remains grounded. 
    // uint32_t deadtime = 0b000000 | (1 << shift[dir][state][0]) ; 
    // uint32_t deadtime = (control == prev_control) ? control : 0b000000 | (1 << shift[dir][state][0]); 

    int deadtime_a;
    int deadtime_b; 
    int deadtime_c;

    if ((control & 0b110000) == (prev_control & 0b110000)) {
        deadtime_a = (control & 0b110000);
    } else {
        deadtime_a = 0b000000;
    }

    if ((control & 0b001100) == (prev_control & 0b001100)) {
        deadtime_b = (control & 0b001100);
    } else {
        deadtime_b = 0b000000;
    }

    if ((control & 0b000011) == (prev_control & 0b000011)) {
        deadtime_c = (control & 0b000011);
    } else {
        deadtime_c = 0b000000;
    }

    uint32_t deadtime = deadtime_a | deadtime_b | deadtime_c ;
    
    uint32_t data = (control << 6) | deadtime;
    pio_sm_put_blocking(gd_pio, gd_sm, data);
    prev_control = control;
}

void irq_handler(uint gpio, uint32_t events) {
    // Update rotor position & gate outputs.
    int a = gpio_get(input_pins[0]);
    int b = gpio_get(input_pins[1]);
    int c = gpio_get(input_pins[2]);
    if ((((a << 2) | (b << 1) | (c)) - 1) != next[dir][state]) { return; }
    state = ((a << 2) | (b << 1) | (c)) - 1;
    update_control();

    // Velocity feedback
    int irq_current_time = time_us_32();

    // time between steps in microseconds
    float step_period = (float)(irq_current_time - irq_prev_time);
    if (step_period <= 800.0f) { // invalid period, ignore
        return;
    }

    irq_prev_time = irq_current_time;

    // step/us * 1 elec. rev/6 steps * 1 mech. rev/4 elec. rev * 1e6 us/s * 60 s/min
    // = 2.5e6 rpm
    float raw_rpm = 2.5e6f / step_period; 

    // low-pass filter
    float alpha = step_period / (TAU + step_period);
    motor_rpm = alpha * raw_rpm + (1.0f - alpha) * motor_rpm;
}

// Update controls on rising & falling edges of PWM signal. 
void pwm_irq0() {
    pio_interrupt_clear(pwm_pio, 0);
    update_control();
}

int main() {
    // Overclocking, be aware that changing the system clock affects the switching dead time!
    set_sys_clock_khz(250000, true) ;
    stdio_init_all();
    multicore_launch_core1(core1_entry); 

    // Highest priority assigned to PIO PWM interrupt
    irq_set_priority(PIO0_IRQ_0, 0);
    irq_set_enabled(PIO0_IRQ_0, true);

    // Second priority assigned to GPIO interrupts for hall sensors    
    irq_set_priority(IO_IRQ_BANK0, 1);
    irq_set_enabled(IO_IRQ_BANK0, true);
    
    // Status LED
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    gpio_put(LED, 1);

    // PWM PIO state machine
    uint pwm_offset = pio_add_program(pwm_pio, &pwm_program);    
    pwm_program_init(pwm_pio, pwm_sm, pwm_offset, PWM_PIN);
  
    irq_set_exclusive_handler(PIO0_IRQ_0, pwm_irq0);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled(pwm_pio, pis_interrupt0, true);

    pio_pwm_set_period(pwm_pio, pwm_sm, WRAPVAL);
    pio_pwm_set_level(pwm_pio, pwm_sm, 0);

    // GATE DRIVE PIO state machine
    uint gd_offset = pio_add_program(gd_pio, &gate_drive_program);
    gate_drive_program_init(gd_pio, gd_sm, gd_offset, OUT_PINS);
    pio_sm_set_enabled(gd_pio, gd_sm, true);
    
    // configure adc for throttle input
    gpio_init(THROTTLE_ADC);
    gpio_set_dir(THROTTLE_ADC, GPIO_IN);
    adc_init();
    adc_gpio_init(THROTTLE_ADC);
    adc_select_input(0);

    // hall effect sensors
    gpio_set_irq_enabled_with_callback(
        input_pins[0], 
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, 
        true, 
        &irq_handler
    );

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        gpio_init(input_pins[i]);
        gpio_set_dir(input_pins[i], GPIO_IN);
        gpio_pull_up(input_pins[i]);
        gpio_set_irq_enabled(
            input_pins[i], 
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
            true
        );
    }  
    int a = gpio_get(input_pins[0]);
    int b = gpio_get(input_pins[1]);
    int c = gpio_get(input_pins[2]);
    state = ((a << 2) | (b << 1) | (c)) - 1;

    uint32_t timer_current_time = time_us_32();

    while (true)
    {
        if (time_us_32() - timer_current_time > timer_us) {
            
            // Timeout for hall input jitter filter.
            int a = gpio_get(input_pins[0]);
            int b = gpio_get(input_pins[1]);
            int c = gpio_get(input_pins[2]);
            state = ((a << 2) | (b << 1) | (c)) - 1;
            update_control();

            // no velocity on boot
            if (irq_prev_time == 0) {
                motor_rpm = 0.0f;
            }

            timer_current_time = time_us_32();
            float timer_period = (float)(timer_current_time - irq_prev_time);
            
            // low-pass filter
            float raw_rpm = 2.5e6f / timer_period; 
            float alpha = timer_period / (TAU + timer_period);
            motor_rpm = alpha * raw_rpm + (1.0f - alpha) * motor_rpm;
            
            // minimum measurable rpm is 25 rpm
            if (motor_rpm < MIN_RPM)
                motor_rpm = 0.0f;
        }

        float throttle = (float) adc_deadzone(adc_read()) / 4095.0f;
        float max_duty = constrainf(motor_rpm / RATED_MOTOR_RPM + MAX_DUTY_AT_STALL, 0.0f, 1.0f);
        float duty = constrainf(throttle * max_duty, 0.0f, 1.0f);
        pio_pwm_set_level(pwm_pio, pwm_sm, (int)(duty * WRAPVAL));

        latest_telem.ts_us = time_us_32();
        latest_telem.motor_rpm = motor_rpm;
        latest_telem.throttle = throttle;
    }
}