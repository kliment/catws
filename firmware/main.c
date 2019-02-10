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
enum {pm_sleep=0, pm_wakeidle, pm_head, pm_neck, pm_back, pm_butt, pm_belly};
#define touch_threshold_on 1
#define touch_threshold_off 1
#define touch_timeout 255

uint16_t bias[5];
uint8_t touch[5];
#if touch_timeout>0
    uint8_t timer[5];
#endif


uint8_t touch_sample(uint8_t sensepin);

void touch_init(void){
    for(uint8_t i=0;i<5;i++){
        bias[i]=touch_sample(i)<<8;
        touch[i]=0;
    }
}


uint8_t touch_sense(uint8_t pad) {
    uint16_t tmp=0;
    int16_t delta;
    for (uint8_t i=0; i<16; i++) {
        tmp+=touch_sample(pad);// average 16 samples
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
        bias[pad]=(bias[pad]-(bias[pad]>>6))+(tmp>>2);// IIR low pass filter
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
    //_delay_us(2); //discharge time
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
    //_delay_us(2); //charge time
    
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

    /* Overflow interrupt enabled */
    TIMSK1 = _BV(TOIE1);
}

#define WAVE_LOG2 8

int8_t purr[1<<WAVE_LOG2] = {0x02,0x04,0x06,0x08,0x0a,0x0b,0x0d,0x0f,0x12,0x14,0x17,0x1b,0x1e,0x21,0x24,0x27,0x2a,0x2d,0x2f,0x31,0x33,0x35,0x37,0x38,0x3a,0x3c,0x3d,0x3f,0x41,0x42,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4a,0x4a,0x4b,0x4b,0x4b,0x4c,0x4d,0x4e,0x4e,0x4f,0x50,0x51,0x51,0x52,0x52,0x52,0x53,0x53,0x54,0x55,0x55,0x56,0x57,0x57,0x58,0x59,0x5a,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x61,0x62,0x62,0x63,0x63,0x63,0x64,0x64,0x64,0x65,0x65,0x66,0x67,0x68,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x6f,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x71,0x71,0x71,0x72,0x72,0x72,0x72,0x72,0x72,0x73,0x73,0x74,0x74,0x75,0x76,0x78,0x79,0x7a,0x7b,0x7b,0x7c,0x7c,0x7d,0x7d,0x7e,0x7e,0x7e,0x7e,0x7f,0x7e,0x7e,0x7e,0x7d,0x7c,0x7c,0x7b,0x7a,0x7a,0x79,0x79,0x79,0x79,0x7a,0x7a,0x7a,0x7a,0x7a,0x7a,0x7a,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x78,0x78,0x78,0x78,0x78,0x78,0x77,0x78,0x78,0x78,0x78,0x79,0x79,0x7a,0x7b,0x7b,0x7c,0x7d,0x7e,0x7e,0x7e,0x7e,0x7e,0x7d,0x7d,0x7c,0x7c,0x7c,0x7b,0x7b,0x7b,0x7b,0x7c,0x7c,0x7b,0x7b,0x7b,0x7a,0x7a,0x79,0x79,0x79,0x78,0x79,0x79,0x79,0x79,0x79,0x76,0x6f,0x62,0x50,0x38,0x19,0xf7,0xd4,0xb4,0x9a,0x89,0x80,0x81,0x88,0x95,0xa4,0xb4,0xc2,0xcd,0xd5,0xda,0xdd,0xde,0xdd,0xde,0xde,0xe0,0xe4,0xe8,0xed,0xf2,0xf7,0xfb,0xff};

static uint16_t phase = 0;
static uint16_t increment = (((uint32_t)1)<<16) * 30. / F_TIM_ISR;
static uint8_t vol = 0;
static uint8_t used_vol = 0;
volatile uint16_t hissing=0;
volatile uint16_t lfsr = 0xACE1u;
volatile uint16_t bit=0;
    
void audio_start(void) {
    /* Prescaler 1 */
    TCCR1B |= _BV(CS10);
    
    /* Interrupt enabled */
    TIMSK1 = _BV(TOIE1);
}
void audio_stop(void){ 
    /* Interrupt disabled */
    TIMSK1 &= ~_BV(TOIE1);
    /* Prescaler 0 -> no clock */
    TCCR1B &= ~_BV(CS10);

    TCNT1 = 0;
    phase = 0;
    OCR1A = 0;
}
ISR(TIMER1_OVF_vect) {
    int16_t val16;
    uint8_t val8;
    if(hissing){
        bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5));
        lfsr = (lfsr >> 1) | (bit << 15);
        val8=lfsr&0xff;
        hissing--;
    }else{
        phase += increment;
        if(phase < increment) {
            if(vol > used_vol) used_vol++;
            if(vol < used_vol) used_vol--;
        }
        val16 = purr[phase>>(16-WAVE_LOG2)];
        val16 *= used_vol;
        val8 = val16 >> 8;
        val8 ^= 0x80;
    }
    OCR1AL = val8;
}

ISR(WDT_vect) {
    __asm("wdr");
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    WDTCSR = 0x00; //turn off watchdog
}


void hiss(){
    PORTD|=_BV(PD6);
    PORTD|=_BV(PD7);
    PORTD&=~_BV(PD5);
    _delay_ms(90);
    hissing=20000;
    vol=250;
    while(hissing)_delay_ms(1);
    vol=0;
    PORTD|=_BV(PD5);
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
    hiss();
    uint8_t wdt_timerflag=0;
    while(1){
        //sleep mode
        vol=0;
        PORTD=0xff; //turn off all LEDs
        cli(); //disable interrupts
        OCR1A=0;//disable sound output;
        PORTB&=~_BV(PB1);//power down piezo
        ADCSRA &=~_BV(ADEN);//disable ADC
        PRR=(_BV(PRADC)|_BV(PRTWI)|_BV(PRTIM1)|_BV(PRTIM0)|_BV(PRSPI));//disable everything else
        if(wdt_timerflag){
            __asm("wdr");
            MCUSR &= ~(1<<WDRF);
            WDTCSR |= (1<<WDCE) | (1<<WDE);//enable changes to watchdog config
            WDTCSR = (1<<WDIE) | (1<<WDP0) | (1<<WDP1);//configure watchdog interrupt after 1/8th of a second
            wdt_timerflag=0;
        }else{
            __asm("wdr");
            MCUSR &= ~(1<<WDRF);
            WDTCSR |= (1<<WDCE) | (1<<WDE);//enable changes to watchdog config
            WDTCSR = (1<<WDIE) | (1<<WDP2);//configure watchdog interrupt after 1/4th of a second
            wdt_timerflag=1;
        }            
        sei();
        
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        MCUCR=(1<<BODSE)|(1<<BODS);
        MCUCR=(1<<BODS);
        sleep_cpu();
        
        PRR &=~_BV(PRADC);
        ADCSRA =_BV(ADEN);//|_BV(ADPS0)|_BV(ADPS1);
        ADCSRA |=_BV(ADSC); // Wake up ADC by doing a conversion
        uint8_t mode=pm_sleep;
        while (!(ADCSRA&_BV(ADIF)));
        CLKPR=_BV(CLKPCE);
        CLKPR=0;
        int16_t happiness=0;
        for(uint8_t i=0;i<5;i++){
            res=touch_sense(i);
            if(res==tt_off){
                PORTD|=_BV(i);
            }else if(res==tt_on){
                PORTD&=~(_BV(i));
                mode=pm_wakeidle;
                if(i==0){//belly touched
                    happiness=-1;
                    mode=pm_belly;
                }
                if(i==4){//head touched
                    happiness=16;
                    mode=pm_head;
                }
                if(i==3){//neck touched
                    happiness=8;
                    mode=pm_neck;
                }
                if(i==2){//back touched
                    happiness=8;
                    mode=pm_back;
                }
                if(i==1){//butt touched
                    happiness=8;
                    mode=pm_butt;
                }
                break;
            }
        }
        if(mode!=pm_wakeidle){
            
            //purr/hiss state machine
            PRR &=~_BV(PRTIM1);
            if(happiness<0){
                hiss();
            }else{
                for(int j=0;j<5*happiness;j++){
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
                            

                            switch(i) {
                                case 0: // belly. Bad unless with grain.
                                    if(mode != pm_butt) {
                                        happiness -= 10;
                                    }
                                    mode = pm_belly;
                                    break;
                                case 1: // Butt. Good with grain, very bad against grain.
                                    if(mode==pm_back){
                                        happiness+=5;
                                    }else if(mode == pm_belly){
                                        happiness-=20;
                                    }else if(mode !=pm_butt){
                                        happiness -=4;
                                    }
                                    mode=pm_butt;
                                    break;
                                case 2: // Back. Good unless against grain.
                                case 3: // Neck. Same.
                                case 4: // Head. Same.
                                    {
                                    uint8_t against_grain = i + 1;
                                    if(against_grain > 4) {
                                        against_grain = 0;
                                    }
                                    against_grain = pm_belly - against_grain;
                                    if(mode != against_grain) {
                                        happiness += 1;
                                    } else {
                                        happiness -= 5;
                                    }
                                    mode = pm_belly - i;
                                    break;
                                    }
                            }
                        }
                    }
                    if(happiness < 0){
                        hiss();
                        break;
                    } else if(happiness&0x100) {
                        happiness=0; //too much stimulation
                    }
                    vol=happiness;
                    if(vol==0) {
                        audio_stop();
                    } else {
                        audio_start();
                    }
                    _delay_ms(9);
                    set_sleep_mode(SLEEP_MODE_IDLE);
                    sleep_enable();
                    sleep_cpu();
                }
            }
            audio_stop();
            mode=pm_sleep;
        }
    }
    return 0;
}
