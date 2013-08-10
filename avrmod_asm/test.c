#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

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


#define BUFFER_SIZE 128

#define USART_IN_BUFFER_SIZE 32
volatile char usart_in_buffer[USART_IN_BUFFER_SIZE];
volatile char usart_in_buffer_pos;
volatile char usart_in_command_complete;

volatile char sample_count;
volatile unsigned int s_ctr;
volatile uint32_t r_ctr;
volatile char flag;
volatile char playing;

//volatile uint8_t * ledstate;
//volatile uint8_t * s;
mix_t * mix;
volatile uint8_t sip;
volatile uint8_t sop;
volatile uint8_t ss;

static const char PROGMEM str_err[] = " ERR\n";
static const char PROGMEM str_ok[] = " OK\n";
static const char PROGMEM str_prompt[] = "$ ";

//char modfile[13];
//#define TESTVECT_LEN 104
//const char testvect[] = "Und nun die Haende zum Himmel komm lasst uns froehlich sein 10 nackte frisoesen wir ham doch keine zeit ";

module_t module;
player_t * player;

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void free_plr() {
    /*
    if (s) {
        free(s);
        s = 0;
    }
    if (ledstate) {
        free(ledstate);
        ledstate = 0;
    }*/
    if (mix) {
        free (mix);
        mix = 0;
    }
    if (player) {
        free(player);
        player = 0;
    }
}

void alloc_plr() {
    free_plr();
    player = (player_t *)malloc(sizeof(player_t));
    //s = (uint8_t *)malloc(BUFFER_SIZE);
    //ledstate = (uint8_t *)malloc(BUFFER_SIZE);
    mix = (mix_t *)malloc(BUFFER_SIZE * sizeof(mix_t));
}

void pwm_init(void) {
    /* use OC1A pin as output */
    DDRD |= _BV(PD5);

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



int main(void) 
{

    int i;

    SRAM_PORT_ADDR_DDR = 0xff;
    SRAM_PORT_DATA_DDR = 0x00;
    SRAM_PORT_DATA = 0x00;

    SRAM_PORT_CTL |= (1 << SRAM_PIN_WE);
    SRAM_PORT_CTL &= ~(1 << SRAM_PIN_OE);
    
    DDRD |= 0b11011100;
    
    DDRB |= 0x0f;
    PORTD &= ~((uint8_t)4);
    
    i = 0;

    sip = 0;
    sop = 0;
    ss = 0;
    
    usart_in_buffer_pos = 0;
    uart_init();

    uart_puts_p(PSTR("\ninit: power on delay (500ms) ... "));
    _delay_ms(1000);
    uart_puts_p(str_ok);


    
    uart_puts_p(PSTR("init: sdcard ... "));
    
    if(!sd_raw_init()) {
#if DEBUG
        uart_puts_p(PSTR("MMC/SD initialization failed\n"));
#endif
        uart_puts_p(str_err);
        for(;;);
    }
    uart_puts_p(str_ok);
  

	s_ctr = 0;
	r_ctr = 0;  
	flag = 0;



    player = 0;
    //s = 0;
    //ledstate = 0;
    mix = 0;
    playing = 0;
    usart_in_command_complete = 0;
    //player_init(player, SAMPLE_RATE);
    //player_set_module(player, &module);
    uart_puts_p(PSTR("init: 1337LofiPWMAudio ... "));
    pwm_init();
    uart_puts_p(str_ok);

	for (i=0; i<32; i++) {
		char c = pgm_read_byte_near(PSTR("Hello World !!!!!!!!!!!!!!!!!!!!!") + i);
		uint32_t x = (uint32_t)i;
		sram_write_char(&x, c);
	}
    
    for (i=0; i<32; i++) {
                uint32_t x = (uint32_t)i;
                char c = sram_read_char(&x);
		uart_putc(c);

    }
    
    uart_puts_p(PSTR("\nVloSoft MOD Player OS 0.1 Ready\n\n"));
    
    uart_puts_p (str_prompt);

    
    
    for (;;) {
        //uint32_t addr;        

        if (playing) {
            if (ss < BUFFER_SIZE) {
                mix[sip++] = player_read(player);
                sip &= (uint8_t)(BUFFER_SIZE - 1);
                ss++;
		//r_ctr++;
            }
        }
	/*
	if (flag) {
		flag = 0;
		uart_putdw_hex(r_ctr);
		uart_putc('\n');
		r_ctr = 0;
	}
    	*/

        if (usart_in_command_complete) {
            usart_in_command_complete = 0;
            uart_putc('\n');
            switch (usart_in_buffer[0]) {
                case 'p':
                    playing = 0;
                    free_plr();
                    
                    for (i = 2; i<USART_IN_BUFFER_SIZE; i++) {
                        if (usart_in_buffer[i] == '\r' || usart_in_buffer[i] == '\n') {
                            usart_in_buffer[i] = 0;
                            break;
                        }
                    }
                    
                    SPSR &= ~(1 << SPIF);
                    
                    loader_mod_loadfile(&module, &usart_in_buffer[2]);
                    
                    alloc_plr();
                    player_init(player, SAMPLE_RATE);
                    player_set_module(player, &module);
                    
                    // prebuffer
                    sip = 0;
                    sop = 0;
                    ss = 0;
                    while (ss < BUFFER_SIZE) {
                        mix[sip++] = player_read(player);
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
                    free_plr();
                    break;
                    
                case 'm':
                    i = freeRam();
                    uart_puts_p(PSTR("Free mem: "));
                    uart_putw_dec(i);
                    uart_puts_p(PSTR("Bytes\n"));
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

    uint16_t spi_data;
    
    if (!playing)
        return;
    
    sample_count--;
    
    if (sample_count == 2) {
        PORTB &= (uint8_t)0xf0;
        PORTB |= mix[sop].ledstate & (uint8_t)15;
    }
    
    if (sample_count == 0) {
        sample_count = SAMPLE_INTERVAL;
        if (ss > 0) {

            spi_data = (mix[sop].output >> 4) | 4096;// >> 4;
            //spi_data |= 4096;
            
            PORTD &= ~((uint8_t)4);
            
            SPSR &= ~(1 << SPIF);
            
            SPDR = (uint8_t)(spi_data >> 8);
            /* wait for byte to be shifted out */
            while(!(SPSR & (1 << SPIF)));
            SPSR &= ~(1 << SPIF);   
            
            SPDR = (uint8_t)spi_data; //mix[sop].output;//s[sop];
            //while(!(SPSR & (1 << SPIF)));
            
            
            sop++;
            sop &= (uint8_t)(BUFFER_SIZE - 1);
            ss--;
            
            PORTD |= ((uint8_t)4);
            
        }
    }
}


ISR(USART_RXC_vect) {
    unsigned char c;

    c = UDR;

    if (usart_in_buffer_pos == USART_IN_BUFFER_SIZE - 2)
            c = '\n';

    if (c == 8) {
        if (usart_in_buffer > 0) {
            usart_in_buffer[--usart_in_buffer_pos] = 0;
            uart_putc(c);
        }
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
