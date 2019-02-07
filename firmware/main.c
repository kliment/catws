#define F_CPU 8000000
#define F_TIM_ISR (F_CPU / 256)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

//#define refpin PC5
//#define refadc ADC5
#define sensepin_x PC4
#define senseadc_x ADC4

enum {tt_off=0,tt_on,tt_push,tt_release,tt_timeout};
#define touch_threshold_on 2
#define touch_threshold_off 1
#define touch_timeout 255

uint16_t bias[5];
uint8_t touch[5];
#if touch_timeout>0
    uint8_t timer[5];
#endif

uint8_t touch_adc(uint8_t sensepin);

void touch_init(void){
    for(int i=0;i<5;i++){
        bias[i]=touch_adc(i)<<8;
        touch[i]=0;
    }
}

uint8_t touch_sense(uint8_t pad) {
    uint8_t i;
    uint16_t tmp;
    int16_t delta;
    tmp=0;
    for (i=0; i<16; i++) {
        tmp+=touch_adc(pad);	// average 16 samples
        _delay_us(100);
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
                bias[pad]=touch_adc(pad)<<8;
                return tt_timeout;
            } 
            timer[pad]++;
        #endif
        return tt_on;
    }
} 

uint8_t touch_adc(uint8_t sensepin){
    uint8_t refpin=5;
    uint8_t refadc=5;
    uint8_t senseadc=sensepin;
    //charge low
    ADMUX = refadc; // connect S/H cap to reference pin
    PORTC |= _BV(refpin); // Charge S/H Cap
    PORTC &=~_BV(sensepin); // Discharge Pad
    DDRC |= _BV(refpin)|_BV(sensepin);
    _delay_us(22); //discharge time
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
    _delay_us(22); //charge time
    
    DDRC  &=~(_BV(sensepin)); // float pad input
    PORTC &=~_BV(sensepin); // disable pullup

    ADMUX =senseadc|_BV(ADLAR); // Connect sense input to adc
    
    ADCSRA |=_BV(ADSC); // Start conversion
    while(!(ADCSRA&_BV(ADIF)));
    ADCSRA |=_BV(ADIF); // Clear ADIF

    uint8_t dat2=ADCH;
    return dat2-dat1;
    
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
}

int main(void){
    PRR &=~_BV(PRADC);
    ADCSRA =_BV(ADEN)|_BV(ADPS0)|_BV(ADPS1);//ADC enabled, prescaler 8
    touch_init();
    audio_init();
    DDRD=_BV(PD5); //output pin to output
    volatile uint8_t res=0;
    
    sei();

    while(1){
        uint8_t new_vol = 0;
        PORTD|=_BV(5);
        PRR &=~_BV(PRADC);
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
        PRR |=_BV(PRADC);
        PORTD&=~(_BV(5));
        _delay_ms(3);
    }
    return 0;
}
