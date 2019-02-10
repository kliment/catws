#define F_CPU 8000000
#define F_TIM_ISR (F_CPU / 256)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


//#define refpin PC5
//#define refadc ADC5
#define sensepin_x PC4
#define senseadc_x ADC4

//#define SENSEMODE_TIME 1

enum {tt_off=0,tt_on,tt_push,tt_release,tt_timeout};
#define touch_threshold_on 2
#define touch_threshold_off 1
#define touch_timeout 255

uint16_t bias[5];
uint8_t touch[5];
#if touch_timeout>0
    uint8_t timer[5];
#endif

uint8_t touch_sample(uint8_t sensepin);

void touch_init(void){
    for(int i=0;i<5;i++){
        bias[i]=touch_sample(i)<<8;
        touch[i]=0;
    }
}


uint8_t touch_sense(uint8_t pad) {
    uint8_t i;
    uint16_t tmp;
    int16_t delta;
    tmp=0;
    for (i=0; i<16; i++) {
        tmp+=touch_sample(pad);	// average 16 samples
        _delay_us(1);
    }
    delta=tmp-(bias[pad]>>4);
    if (!touch[pad]){
        if (delta>touch_threshold_on) {
            touch[pad]=1;
        #if touch_timeout>0
            timer[pad]=0;
        #endif
            return tt_push;
        }

        // update bias only when touch not active
        bias[pad]=(bias[pad]-(bias[pad]>>6))+(tmp>>2);		// IIR low pass
        return tt_off;
    } else {
        if (delta<touch_threshold_off) {
            touch[pad]=0;
            return tt_release;
        }

        #if touch_timeout>0
            if (timer[pad]==255) {
                bias[pad]=touch_sample(pad)<<8;
                return tt_timeout;
            } 
            timer[pad]++;
        #endif
        return tt_on;
    }
} 

uint8_t touch_sample(uint8_t sensepin){
#ifndef SENSEMODE_TIME
    uint8_t refpin=5;
    uint8_t refadc=5;
    uint8_t senseadc=sensepin;
    //charge low
    ADMUX = refadc; // connect S/H cap to reference pin
    PORTC |= _BV(refpin); // Charge S/H Cap
    PORTC &=~_BV(sensepin); // Discharge Pad
    DDRC |= _BV(refpin)|_BV(sensepin);
    _delay_us(2); //discharge time
    DDRC &=~(_BV(sensepin)); // float pad input, pull up is off.
    ADMUX =senseadc|_BV(ADLAR); //connect sense input to ADC
    ADCSRA |=_BV(ADSC); // Start conversion
    while (!(ADCSRA&_BV(ADIF)));
    ADCSRA |=_BV(ADIF); // Clear ADIF
    uint8_t dat1=ADCH;
    ADMUX =refadc; // connect S/H cap to reference pin
    PORTC &=~_BV(refpin); // Discharge S/H Cap
    PORTC |= _BV(sensepin); // Charge Pad 
    DDRC  |= _BV(refpin)|_BV(sensepin);
    _delay_us(2); //charge time
    
    DDRC  &=~(_BV(sensepin)); // float pad input
    PORTC &=~_BV(sensepin); // disable pullup

    ADMUX =senseadc|_BV(ADLAR); // Connect sense input to adc
    
    ADCSRA |=_BV(ADSC); // Start conversion
    while(!(ADCSRA&_BV(ADIF)));
    ADCSRA |=_BV(ADIF); // Clear ADIF

    uint8_t dat2=ADCH;
    return dat2-dat1;
#else
    //discharge pin
    PORTC &= ~_BV(sensepin);
    DDRC |= _BV(sensepin);
    __asm("nop");

    uint8_t cycles = 17;
    volatile uint8_t* pin=&(PINC);
    
    // Prevent the timer IRQ from disturbing our measurement
    
    cli();
                
    // Make the pin an input with the internal pull-up on
    DDRC &= ~_BV(sensepin);
    PORTC |= _BV(sensepin);
    
    // Now see how long the pin to get pulled up.
    
    for(uint8_t i;i<17;i++){
        if(PINC&_BV(sensepin)){
            cycles=i;
            break;
        }
    }

    sei();
    
    // Discharge the pin again by setting it low and output
    //  It's important to leave the pins low if you want to 
    //  be able to touch more than 1 sensor at a time - if
    //  the sensor is left pulled high, when you touch
    //  two sensors, your body will transfer the charge between
    //  sensors.
    
    PORTC &= ~_BV(sensepin);
    DDRC  |= _BV(sensepin);
    
    return cycles;
#endif    
}

void audio_init(void){
    /* Clock prescaler to 1. */
    CLKPR=_BV(CLKPCE);
    CLKPR=0;

    /* PWM pin PB1 as output */
    DDRB |= _BV(1);

    /* Fast PWM, 8 bit. */
    TCCR1A |= _BV(WGM10);
    TCCR1B |= _BV(WGM12);

    /* Toggle OC1A on compare match. */
    TCCR1A |= _BV(COM1A1);

    /* Prescaler 1 */
    TCCR1B |= _BV(CS10);

    /* No output for now */
    OCR1A = 0;

    /* Overflow interrupt enabled */
    TIMSK1 = _BV(TOIE1);
}

#define WAVE_LOG2 8

uint8_t purr[1<<WAVE_LOG2] = {0x80,0x74,0x64,0x52,0x41,0x31,0x24,0x1b,0x17,0x17,0x1b,0x21,0x29,0x31,0x38,0x3d,0x41,0x44,0x45,0x46,0x46,0x46,0x46,0x47,0x49,0x4b,0x4d,0x50,0x52,0x55,0x57,0x58,0x5a,0x5b,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x62,0x63,0x65,0x67,0x68,0x6a,0x6b,0x6d,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x76,0x77,0x78,0x79,0x7a,0x7a,0x7b,0x7b,0x7c,0x7c,0x7c,0x7d,0x7d,0x7d,0x7d,0x7d,0x7e,0x7e,0x7f,0x7f,0x7f,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x81,0x81,0x81,0x81,0x82,0x82,0x82,0x83,0x83,0x84,0x85,0x85,0x86,0x86,0x87,0x87,0x88,0x88,0x88,0x88,0x88,0x88,0x89,0x89,0x89,0x89,0x8a,0x8a,0x8a,0x8b,0x8c,0x8c,0x8d,0x8d,0x8e,0x8e,0x8e,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x8f,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x91,0x91,0x92,0x92,0x93,0x93,0x94,0x94,0x95,0x95,0x95,0x95,0x96,0x96,0x96,0x96,0x96,0x96,0x96,0x96,0x96,0x95,0x95,0x95,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x94,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x94,0x94,0x95,0x95,0x95,0x96,0x96,0x96,0x96,0x96,0x96,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x95,0x94,0x94,0x94,0x94,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x93,0x92,0x8e,0x88};

static uint16_t phase = 0;
static uint16_t increment = (((uint32_t)1)<<16) * 30. / F_TIM_ISR;
static uint8_t vol = 0;

ISR(TIMER1_OVF_vect) {
    phase += increment;
    int8_t val = purr[phase>>(16-WAVE_LOG2)] - 128;
    int16_t val16 = val;
    val16 *= vol;
    int8_t new_ocr = val16 >> 8;
    OCR1A = new_ocr ^ 0x80;
    //OCR1A = (phase & 0x8000) ? 100 : 0;// use this for square wave output
}

ISR(WDT_vect) {
    __asm("wdr");
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    WDTCSR = 0x00; //turn off watchdog
}

int main(void){
#ifndef SENSEMODE_TIME
    PRR &=~_BV(PRADC);
    ADCSRA =_BV(ADEN)|_BV(ADPS0)|_BV(ADPS1);//ADC enabled, prescaler 8
#endif
    touch_init();
    CLKPR=_BV(CLKPCE);
    CLKPR=0;
    //audio_init();
    DDRD|=_BV(PD5); //output pin to output
    DDRD|=_BV(PD6); //output pin to output
    DDRD|=_BV(PD7); //output pin to output
    DDRB=_BV(PB1); //output pin to output
    PORTD|=_BV(PD6);
    PORTD|=_BV(PD7);
        
    volatile uint8_t res=0;
    
    sei();
    PORTD&=~_BV(PD5);
    
    for(uint16_t i=0;i<1250;i++){
        PORTB|=_BV(PB1);
        for(uint8_t j=0;j<i*19%7;j++)_delay_us(25);
        PORTB&=~_BV(PB1);
        for(uint8_t j=0;j<i*13%11;j++)_delay_us(50);
        
    }
    PORTD&=~_BV(PD6);
    PORTD&=~_BV(PD7);
    for(uint8_t i=0;i<5;i++){
        for(uint8_t j=0;j<5;j++)
            touch_sense(j);
    }
    for(uint8_t purrs=0;purrs<1;purrs++){
        for(uint8_t i=0;i<20;i++){
            PORTB|=_BV(PB1);
            _delay_ms(25);
            PORTB&=~_BV(PB1);
            _delay_ms(25);
        }
        for(uint8_t i=0;i<25;i++){
            PORTB|=_BV(PB1);
            _delay_ms(20);
            PORTB&=~_BV(PB1);
            _delay_ms(20);
        }
    }
    
    audio_init();
    
    while(1){
        //sleep mode
        PORTD=0xff; //turn off all LEDs
        cli(); //disable interrupts
        OCR1A=0;//disable sound output;
        PORTB&=~_BV(PB1);//power down piezo
        ADCSRA &=~_BV(ADEN);//disable ADC
        PRR|=(_BV(PRADC)|_BV(PRTWI)|_BV(PRTIM1)|_BV(PRTIM0)|_BV(PRSPI));//disable everything else
        __asm("wdr");
        MCUSR &= ~(1<<WDRF);
        WDTCSR |= (1<<WDCE) | (1<<WDE);//enable changes to watchdog config
        WDTCSR = (1<<WDIE) | (1<<WDP2);//configure watchdog interrupt after 1/4th of a second
        sei();
        
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        MCUCR=(1<<BODSE)|(1<<BODS);
        MCUCR=(1<<BODS);
        sleep_cpu();
        
        PRR &=~_BV(PRADC);
        ADCSRA =_BV(ADEN);//|_BV(ADPS0)|_BV(ADPS1);
        ADCSRA |=_BV(ADSC); // Wake up ADC by doing a conversion
        uint8_t wkup_needed=0;
        while (!(ADCSRA&_BV(ADIF)));
        CLKPR=_BV(CLKPCE);
        CLKPR=0;
        for(uint8_t i=0;i<5;i++){
            res=touch_sense(i);
            if(res==tt_off){
                PORTD|=_BV(i);
            }else if(res==tt_on){
                PORTD&=~(_BV(i));
                wkup_needed=1;
            }
        }
        
        if(wkup_needed){
            wkup_needed=0;
            PRR &=~_BV(PRTIM1);
            for(int j=0;j<750;j++){
                for(uint8_t i=0;i<5;i++){
                    DDRD|=_BV(i);
                    res=touch_sense(i);
                    if(res==tt_off){
                        PORTD|=_BV(i);
                    }else if(res==tt_on){
                        PORTD&=~(_BV(i));
                        j=0;
                        if(vol<250)
                            vol += 50;
                        PORTD&=~_BV(PD6);
                        PORTD&=~_BV(PD5);
                        PORTD&=~_BV(PD7);
    
                    }
                }
                set_sleep_mode(SLEEP_MODE_IDLE);
                sleep_enable();
                sleep_cpu();
            }
        }
        //vol = new_vol
        
        //CLKPR=_BV(CLKPCE);
        //CLKPR=8; // divide clock by 256 - running at 31.25 kHz, 32us/cycle
        //sleep for 256ms - because of clock scaling this is equivalent to 1ms macro sleep
        //_delay_ms(1);
        //CLKPR=_BV(CLKPCE);
        //CLKPR=0; //speed up clock again
        //PORTD&=~_BV(PD6);
        
    }
        
        
        
    

    while(0){
        uint8_t new_vol = 0;
        PORTD|=_BV(5);
        PRR &=~_BV(PRADC);
        ADCSRA =_BV(ADEN)|_BV(ADPS0)|_BV(ADPS1);
        ADCSRA |=_BV(ADSC); // Wake up ADC by doing a conversion
        while (!(ADCSRA&_BV(ADIF)));
    
        for(int i=0;i<5;i++){
            DDRD=_BV(i);
            res=touch_sense(i);
            if(res==tt_off){
                PORTD|=_BV(i);
            }else if(res==tt_on){
                PORTD&=~(_BV(i));
                new_vol += 50;
            }
        }
        vol = new_vol;
#ifndef SENSEMODE_TIME
        PRR |=_BV(PRADC);
        ADCSRA &=~_BV(ADEN);
#endif
        PORTD&=~(_BV(5));
        _delay_ms(3);
    }
    return 0;
}
