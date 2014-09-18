#include "Arduino.h"
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>


// Enable this to have info output on Serial when running.
#undef TCDEBUG

// Coloured LEDs are set depending on battery status
// Adjust these to match your own setup.
int redled = 17; // built-in LED on arduino micro
int greenled = 9;

// The pin the (divided) battery voltage is attached to:
int sensorPin = A1;

// The pin the light sensor is attached to:
int lightPin = A2;

// OutputPins
// Pin A - enabled when there's enough power and no light
int outputPinA = 5;
// Pin B - enabled when there's enough power.
int outputPinB = 6;


bool long_running = false;

void led_setup(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);
}

void enable_relay(int pin) {
    digitalWrite(pin, LOW);
}

void disable_relay(int pin) {
    digitalWrite(pin, HIGH);
}

void blank_leds() {
    digitalWrite(redled, LOW);
    digitalWrite(greenled, LOW);
}

void enable_one_led(int pin) {
    blank_leds();
    digitalWrite(pin, HIGH);
}

void flash_one_led(int pin) {
    static int status = 0;
    blank_leds();
    if (status) {
        status=0;
    }
    else {
        status = 1;
        digitalWrite(pin, HIGH);
    }
}

void setup() {
    int leds[] = { redled, greenled };

    // Enable INPUT+pull-up on all pins; this does save ~10mA
    for (int i=0; i <= 19; i++) {
        pinMode(i, INPUT);
        digitalWrite(i, HIGH); // enables pull-up on input
    }
    
    power_spi_disable();

    // Now setup the ones we actually want to output upon..
    pinMode(outputPinA, OUTPUT);
    pinMode(outputPinB, OUTPUT);
    disable_relay(outputPinA);
    disable_relay(outputPinB);
    for (int i=0; i<2; i++) {
        led_setup(leds[i]);
    }
#ifdef TCDEBUG
    Serial.begin(38400);
#endif
}

// Low Power Delay.  Drops the system clock
// to its lowest setting and sleeps for 256*quarterSeconds milliseconds.
// Except in my actual experience, this doesn't seem to work as described.
// It definitely drops power draw, but doesn't take anywhere near as long as n*256 ms
// and large values of n seem to crash my board. WTF?
void lpDelay(int quarterSeconds) {
    // clock_div_t previous_speed;
    // previous_speed = clock_prescale_get();

    delay(16); // required or else shit seems to go wrong

    clock_prescale_set(clock_div_256); // 1/256 speed == 60KHz

    delay(quarterSeconds);  // since the clock is slowed way down, delay(n) now acts like delay(n*256)

    //clock_prescale_set(previous_speed); // restore original speed
    clock_prescale_set(clock_div_1); // back to full speed

    delay(16); // required or else shit seems to go wrong
}


/*
  Here follows code to drop the Arduino into sleep mode until it's woken
  up by a time-out at 8 seconds.
  Unfortunately.. it didn't seem to work properly for me, and I haven't
  had time to work out why.
*/

ISR(WDT_vect)
{
  // does nothing
}

int watchdog_enabled = 0;
void enable_watchdog() {
  // Clear the reset flag, the WDRF bit (bit 3) of MCUSR.
  MCUSR = MCUSR & B11110111;
  
// Set the WDCE bit (bit 4) and the WDE bit (bit 3) 
// of WDTCSR. The WDCE bit must be set in order to 
// change WDE or the watchdog prescalers. Setting the 
// WDCE bit will allow updtaes to the prescalers and 
// WDE for 4 clock cycles then it will be reset by 
// hardware.
  WDTCSR = WDTCSR | B00011000; 

  // Set the watchdog timeout prescaler value to 1024 K 
  // which will yeild a time-out interval of about 8.0 s.
  WDTCSR = B00100001;

  // Enable the watchdog timer interupt.
  WDTCSR = WDTCSR | B01000000;
  MCUSR = MCUSR & B11110111;
}

void enterSleep(void)
{
  if (watchdog_enabled == 0) {
    enable_watchdog();
    watchdog_enabled = 1;
  }
  set_sleep_mode(SLEEP_MODE_PWR_SAVE);
  sleep_enable();
  sleep_mode();
  
  sleep_disable(); /* First thing to do is disable sleep. */
  delay(100); // let things settle
}

/* End of sleep-mode code */


void loop() {
    int lightAcc = 0;
    int voltageAcc = 0;
    float voltage, light;

    voltageAcc += analogRead(sensorPin);
    lightAcc += analogRead(lightPin);        
    delay(25);
    voltageAcc += analogRead(sensorPin);
    lightAcc += analogRead(lightPin);        
    delay(25);
    voltageAcc += analogRead(sensorPin);
    lightAcc += analogRead(lightPin);        

    light = float(lightAcc)/float(3.0);
    
#ifdef TCDEBUG
    Serial.print("Average voltage ADC: ");
    Serial.println(voltageAcc/3.0);
    Serial.print("Average light ADC: ");
    Serial.println(light);    
#endif

    // Using a voltage divider means we get a third of the real
    // value. We then take three readings, though, accumulating them.
    // Multiply by 5 given that analogRead's 1024=5V
    // Then finally divide the result out to get to the 12V reading.
    voltage = float(voltageAcc) / float(212.0);

#ifdef TCDEBUG
    Serial.print("Voltage: ");
    Serial.println(voltage); // automatically fixed to 2 decimal places
#endif  


/*
   The objective of this section is to switch the relays on and off depending
   on voltage. There is a gap between the turn-on and turn-off points so as
   to avoid flapping between the two states when close, as the voltage will
   read higher when there is no load connected compared to when there is.
*/

    if (light > 400 || voltage > 12.9) {
        // ie. We're charging, so there's lots of sun
        // So disable the lighting relay.
        flash_one_led(greenled);
        disable_relay(outputPinA);
        enable_relay(outputPinB);
    }
    else if (voltage > 12.3) {
        enable_one_led(greenled);
        enable_relay(outputPinA);
        enable_relay(outputPinB);
    }
    else if (voltage < 11.8) {
        flash_one_led(redled);
        disable_relay(outputPinA);
        disable_relay(outputPinB);
    }
    else {
        // between 11.8 and 12.3:
        enable_one_led(redled);
    }

    // Delay for longer if we're running on very low power, but only after
    // we've been running for long enough to reprogram or power settles.
    if (millis() < 12000) {
        delay(2000);
    }
    else {
#ifdef TCDEBUG
        Serial.println("pausing");
        delay(2000);
#else
        lpDelay(20);
        //enterSleep();
#endif
    }
}

