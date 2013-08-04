#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "sram.h"
#include "mmc/uart.h"

#include "modplay/module.h"
#include "modplay/player.h"
#include "modplay/loader_mod.h"
#include <stdlib.h>

#define SRAM_SIZE 524288ul

#define SAMPLE_RATE     16000
#define SAMPLE_INTERVAL 64000 / SAMPLE_RATE

#define BUFFER_SIZE 64

volatile char sample_count;
volatile unsigned int s_ctr;
volatile uint32_t r_ctr;
volatile char flag;

volatile uint8_t s[BUFFER_SIZE];
volatile char sip;
volatile char sop;
volatile char ss;

//#define TESTVECT_LEN 104
//const char testvect[] = "Und nun die Haende zum Himmel komm lasst uns froehlich sein 10 nackte frisoesen wir ham doch keine zeit ";

module_t module;
player_t * player;

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


int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


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
    sample_count = SAMPLE_INTERVAL;
    sei(); //Enable interrupts
}

int main(void) {

    uint32_t addr = 0x0;
    int i, j, k;
    char c;

    DDRA = 0xff;
    DDRC = 0xff;
    //PORTC |= SRAM_PIN_WE;


    i = 0;

    s_ctr = 0;
    r_ctr = 0;

    sip = 0;
    sop = 0;
    ss = 0;
    
    uart_init();

    _delay_ms(1000);

    /*
    for (r_ctr = 0; r_ctr < SRAM_SIZE; r_ctr++) {
        sram_write_char(&r_ctr, testvect[r_ctr % TESTVECT_LEN]);
    }
    
    for (r_ctr = 0; r_ctr < SRAM_SIZE; r_ctr++) {
        c = sram_read_char(&r_ctr);
        if (c != testvect[r_ctr % TESTVECT_LEN]) {
            uart_puts("erreur: ");
            uart_putdw_hex(r_ctr);
            uart_putc('\n');
        }
            
    }
    */
    uart_puts_p(PSTR("loading mod ..."));
    
    loader_mod_loadfile(&module, "class02.mod");
    
    uart_puts_p(PSTR(" done\r\n"));

    uart_puts_p(PSTR("MEM before alloc: "));
    i = freeRam();
    uart_putw_dec(i);
    
    i = sizeof(player_t);
    uart_puts_p(PSTR(", Player size: "));
    uart_putw_dec(i);
   

    player = (player_t *)malloc(sizeof(player_t));
    
    player_init(player, SAMPLE_RATE);
    player_set_module(player, &module);

    uart_puts_p(PSTR(", MEM after alloc: "));
    i = freeRam();
    uart_putw_dec(i);
       
    

    /*
    for (i=0; i<31; i++) {
        uart_putdw_hex(module.sample_headers[i].sram_offset);
        uart_putc(' ');
        uart_putdw_hex(module.sample_headers[i].length);
        uart_putc('\n');
    }
     */
    /*
    for (i=0; i<module.num_patterns; i++) {
        for (j = 0; j < 64; j++) {
            for (k=0; k<4; k++) {
                c = sram_read_char(&addr);
                addr++;
                uart_putc_hex(c);
                c = sram_read_char(&addr);
                addr++;
                uart_putc_hex(c);
                c = sram_read_char(&addr);
                addr++;
                uart_putc_hex(c);
                c = sram_read_char(&addr);
                addr++;
                uart_putc_hex(c);
                uart_putc(' ');
            }
            uart_putc('\n');
        }
        uart_putc('\n');
    }
    
    //pwm_init();
    */
    
    
    
    sip = 0;
    while (ss < BUFFER_SIZE) {
        player_read(player, (uint8_t *)&(s[sip++]));
        sip &= (uint8_t)(BUFFER_SIZE - 1);
        ss++;
    }
    
    pwm_init();
    
    i = 0;
    for (;;) {
        //uint32_t addr;        

        if (ss < BUFFER_SIZE) {

            //addr = module.sample_headers[i].sram_offset + r_ctr;

            player_read(player, (uint8_t *)&(s[sip++]));
            //s[sip++] = sram_read_char(&addr);
            sip &= (uint8_t)(BUFFER_SIZE - 1);
            ss++;
            
            r_ctr++;
            if (r_ctr >= module.sample_headers[i].length)
                r_ctr = 0;

        }
        
        
    }


}

ISR(TIMER0_OVF_vect) {

    sample_count--;
    
    if (sample_count == 0) {
        sample_count = SAMPLE_INTERVAL;
        if (ss > 0) {
            OCR1A = s[sop++];
            sop &= (uint8_t)(BUFFER_SIZE - 1);
            ss--;
        }
        //if(sample > pcm_length)
        //	sample=0;
        //s_ctr++;
        //if (s_ctr == 16000) {
        //	flag = 1;
        //	s_ctr = 0;
        //}

    }
}
