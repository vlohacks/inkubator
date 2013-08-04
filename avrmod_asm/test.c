#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "sram.h"
#include "mmc/uart.h"
#include "mmc/sd_raw.h"

#include "modplay/module.h"
#include "modplay/player.h"
#include "modplay/loader_mod.h"
#include <stdlib.h>

#define SRAM_SIZE 524288ul

#define SAMPLE_RATE     16000
#define SAMPLE_INTERVAL 64000 / SAMPLE_RATE


#define BUFFER_SIZE 64

#define USART_IN_BUFFER_SIZE 32
volatile char usart_in_buffer[USART_IN_BUFFER_SIZE];
volatile char usart_in_buffer_pos;
volatile char usart_in_command_complete;

volatile char sample_count;
//volatile unsigned int s_ctr;
//volatile uint32_t r_ctr;
volatile char flag;
volatile char playing;

volatile uint8_t s[BUFFER_SIZE];
volatile char sip;
volatile char sop;
volatile char ss;

static const char PROGMEM str_err[] = " ERR\n";
static const char PROGMEM str_ok[] = " OK\n";
static const char PROGMEM str_prompt[] = "$ ";

//char modfile[13];
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

    int i;
    char c;

    DDRA = 0xff;
    DDRC = 0xff;

    i = 0;

    sip = 0;
    sop = 0;
    ss = 0;
    playing = 0;
    
    usart_in_buffer_pos = 0;
    uart_init();

    uart_puts_p(PSTR("\ninit: power on delay (500ms) ... \n"));
    _delay_ms(500);
    uart_puts_p(str_ok);
    
    uart_puts_p(PSTR("init: sdcard ... "));
    
    if(!sd_raw_init()) {
#if DEBUG
        uart_puts_p(PSTR("MMC/SD initialization failed\n"));
#endif
        //return 1;
        uart_puts_p(str_err);
        for(;;);
    }
    uart_puts_p(str_ok);
    



    player = 0;
    playing = 0;
    usart_in_command_complete = 0;
    //player_init(player, SAMPLE_RATE);
    //player_set_module(player, &module);
    uart_puts_p(PSTR("init: 1337LofiPWMAudio ... "));
    pwm_init();
    uart_puts_p(str_ok);
    
    uart_puts_p(PSTR("\nVloSoft MOD Player OS 0.1 Ready\n\n"));
    
    uart_puts_p (str_prompt);

    
    
    for (;;) {
        //uint32_t addr;        

        if (playing) {
            if (ss < BUFFER_SIZE) {

                //addr = module.sample_headers[i].sram_offset + r_ctr;

                player_read(player, (uint8_t *)&(s[sip++]));
                //s[sip++] = sram_read_char(&addr);
                sip &= (uint8_t)(BUFFER_SIZE - 1);
                ss++;
                //PORTB &= 0xf0;
                //PORTB |= ((1 << (ss >> 4)) - 1);
                //r_ctr++;
                //if (r_ctr >= module.sample_headers[i].length)
                //    r_ctr = 0;

            }
        }
    
        if (usart_in_command_complete) {
            usart_in_command_complete = 0;
            uart_putc('\n');
            switch (usart_in_buffer[0]) {
                case 'p':
                    playing = 0;
                    if (player) {
                        free(player);
                        player = 0;
                    }
                    
                    for (i = 2; i<USART_IN_BUFFER_SIZE; i++) {
                        if (usart_in_buffer[i] == '\r' || usart_in_buffer[i] == '\n') {
                            usart_in_buffer[i] = 0;
                            break;
                        }
                    }
                        
                    
                    loader_mod_loadfile(&module, &usart_in_buffer[2]);
                    
                    player = (player_t *)malloc(sizeof(player_t));
                    player_init(player, SAMPLE_RATE);
                    player_set_module(player, &module);
                    
                    // prebuffer
                    sip = 0;
                    sop = 0;
                    ss = 0;
                    while (ss < BUFFER_SIZE) {
                        player_read(player, (uint8_t *)&(s[sip++]));
                        sip &= (uint8_t)(BUFFER_SIZE - 1);
                        ss++;
                    }                    
                    
                    playing = 1;
                    
                    break;
                    
                case 'l':

                    if (playing) {
                        uart_puts_p(PSTR("sorry pal, not enough mem for ls while playing :-/\n"));
                        break;
                    }
                        
                    loader_mod_ls();

                    break;
                    
                case 's':
                    playing = 0;
                    if (player) {
                        free(player);
                        player = 0;
                    }
                    break;
                    
                case 'm':
                    i = freeRam();
                    uart_puts_p(PSTR("Free mem: "));
                    uart_putw_dec(i);
                    uart_putc(' Bytes\n');
                    break;
                        
                        
                    
                default:
                    if (usart_in_buffer[0] != '\r' && usart_in_buffer[0] != '\n')
                        uart_puts_p(PSTR("invalid command\n"));
                    break;
            }
            
            uart_puts_p (str_prompt);
        }
        
        
    }


}

ISR(TIMER0_OVF_vect) {

    if (!playing)
        return;
    
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


ISR(USART_RXC_vect) {
	unsigned char c;

	c = UDR;

	if (usart_in_buffer_pos == USART_IN_BUFFER_SIZE - 2)
		c = '\n';
	
        if (c == 8) {
            usart_in_buffer[--usart_in_buffer_pos] = 0;
            uart_putc(c);
            return;
        }
            
	usart_in_buffer[usart_in_buffer_pos++] = c;
        uart_putc(c);

	if (c == '\n' || c == '\r') {
		usart_in_buffer[usart_in_buffer_pos] = '\0';
		usart_in_buffer_pos = 0;
                usart_in_command_complete = 1;
	}
	
}