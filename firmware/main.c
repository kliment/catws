#define F_CPU 1000000
#include <avr/io.h>
#include <util/delay.h>

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

int main(void){
    PRR &=~_BV(PRADC);
    ADCSRA =_BV(ADEN)|_BV(ADPS0)|_BV(ADPS1);//ADC enabled, prescaler 8
    touch_init();
    DDRD=_BV(PD5); //output pin to output
    //DDRB=_BV(PB1); //output pin to output
    volatile uint8_t res=0;
    
    while(1){
        PORTD|=_BV(5);
        CLKPR=_BV(CLKPCE);
        CLKPR=0;
        PRR &=~_BV(PRADC);
        for(int i=0;i<5;i++){
            DDRD=_BV(i);
            res=touch_sense(i);
            if(res==tt_off){
                PORTD|=_BV(i);
            }else if(res==tt_on){
                PORTD&=~(_BV(i));
            }
        }
        PRR |=_BV(PRADC);
        PORTD&=~(_BV(5));
        CLKPR=_BV(CLKPCE);
        CLKPR=8;
        _delay_ms(3);
    }
    return 0;
}
