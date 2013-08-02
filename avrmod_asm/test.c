#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "sram.h"
#include "mmc/uart.h"

#include "modplay/module.h"
#include "modplay/loader_mod.h"

#define TESTVECT_LEN 52
#define SRAM_SIZE 524288ul

const char testvect[] = "Der Sunny ist ein Drecksack, weil er immer raus will";

volatile char sample_count;
volatile unsigned int s_ctr;
volatile uint32_t r_ctr;
volatile char flag;

module_t module;
/*
inline void c_latch(char * addr) {
    DDRA = 0xff;
    PORTC |= 7;
    PORTA = *addr++;
    PORTC &= (unsigned char) (~1);
    PORTA = *addr++;
    PORTC &= (unsigned char) (~2);
    PORTA = *addr++;
    PORTC &= (unsigned char) (~4);
}

void c_sram_write_char(char * addr, char c) {
    c_latch(addr);
    PORTA = c;
    PORTC &= ~(1 << SRAM_PIN_WE);
    asm("nop");
    asm("nop");
    PORTC |= (1 << SRAM_PIN_WE);
}

char c_sram_read_char(char * addr) {
    char c;
    c_latch(addr);
    DDRA = 0x00;
    PORTC &= ~(1 << SRAM_PIN_OE);
    asm("nop");
    asm("nop");
    c = PINA;
    PORTC |= (1 << SRAM_PIN_OE);
    return c;
}
*/
void pwm_init(void) {
    /* use OC1A pin as output */
    DDRD = _BV(PD5);

    /*
     * clear OC1A on compare match
     * set OC1A at BOTTOM, non-inverting mode
     * Fast PWM, 8bit
     */
    TCCR1A = _BV(COM1A1) | _BV(WGM10);

    /*
     * Fast PWM, 8bit
     * Prescaler: clk/1 = 8MHz
     * PWM frequency = 8MHz / (255 + 1) = 31.25kHz
     */
    TCCR1B = _BV(WGM12) | _BV(CS10);

    /* set initial duty cycle to zero */
    OCR1A = 0;

    /* Setup Timer0 */

    TCCR0 |= (1 << CS00);
    TCNT0 = 0;
    TIMSK |= (1 << TOIE0);
    sample_count = 4;
    sei(); //Enable interrupts
}

int main(void) {

    uint32_t addr = 0x0;
    int i;
    char c;

    DDRA = 0xff;
    DDRC = 0xff;
    //PORTC |= SRAM_PIN_WE;


    i = 0;
    int err = 0;

    s_ctr = 0;
    r_ctr = 0;

    uart_init();

    _delay_ms(1000);

    //uart_puts("latch\r\n");

    //addr = 0b0101010111001100;
    //sram_latch(&addr);

    //for (;;);

    uart_puts("==write==\r\n");
    for (addr = 0; addr < 512; addr++) {
        uart_putc(testvect[i]);
        sram_write_char(&addr, testvect[i++]);
        if (i >= TESTVECT_LEN)
            i = 0;
        //_delay_ms(20);

    }

    uart_puts("==read==\r\n");
    for (addr = 0; addr < 512; addr++) {
        c = sram_read_char(&addr);
        uart_putc(c);
        //_delay_ms(20);
    }

    //pwm_init();	
    
    uart_puts("loading mod ...");
    
    loader_mod_loadfile(&module, "aurora.mod");
    
    uart_puts(" done\r\n");
    
    pwm_init();
    
    for (;;);
    
    for (;;) {

        uart_puts("memtest: fill\r\n");

        i = 0;
        for (addr = 0; addr < SRAM_SIZE; addr++) {
            sram_write_char(&addr, testvect[addr % TESTVECT_LEN]);
        }

        err = 0;
        uart_puts("memtest: verify\r\n");

        for (addr = 0; addr < SRAM_SIZE; addr++) {
            c = sram_read_char(&addr);
            if (c != testvect[addr % TESTVECT_LEN]) {
                uart_puts("oh no, error @ ");
                uart_putw_hex(addr);
                uart_puts("\r\n");
                err++;
            }
        }

        uart_puts("done, errors: ");
        uart_putw_dec(err);
        uart_puts("\r\n");

        /*

        sram_read_char(&addr);
        addr++;
        r_ctr++;
        if (flag) {
                flag = 0;
                uart_puts ("read perf: ");
                uart_putdw_dec(r_ctr);
                uart_puts("\r\n");
                r_ctr = 0;
        }
         */

    }


}

ISR(TIMER0_OVF_vect) {

    sample_count--;
    if (sample_count == 0) {
        sample_count = 4;
        OCR1A = (unsigned char)(sram_read_char(&r_ctr) + 128);
        r_ctr ++;
        //if(sample > pcm_length)
        //	sample=0;
        //s_ctr++;
        //if (s_ctr == 16000) {
        //	flag = 1;
        //	s_ctr = 0;
        //}

    }
}
