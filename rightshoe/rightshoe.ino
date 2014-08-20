// 'Firewalker' LED sneakers sketch for Adafruit NeoPixels by Phillip Burgess
// Uses self-calibrating code by Jeff H.
// Switches between modes every time power is turned off
// Each mode displays arbitrary user-defined colors

#include <Adafruit_NeoPixel.h>
#include <elapsedMillis.h>
#include <EEPROM.h>

// User-defined colors. Add more if desired
const uint8_t BLACK[] = {0,0,0};
const uint8_t RED[] = {255,0,0};
const uint8_t GREEN[] = {0,255,0};
const uint8_t BLUE[] = {0,0,255};
const uint8_t YELLOW[] = {255,255,0};
const uint8_t MAGENTA[] = {255,0,255};
const uint8_t CYAN[] = {0,255,255};
const uint8_t WHITE[] = {255,255,255};

// List of program mode names. You can add new modes here. Modes are defined in function setMode()
typedef enum {MODE_FIRE, MODE_RGB, MODE_CMY, MODE_BGY} progmode;
#define MODEADDRESS 100 // Byte address in EEPROM to use for the mode counter (0-1023).

// Gamma correction table for LED brightness
uint8_t gamma[] PROGMEM = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
  2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
  5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };
  
// LEDs go around the full perimeter of the shoe sole, but the step animation
// is mirrored on both the inside and outside faces, while the strip doesn't
// necessarily start and end at the heel or toe. These constants help configure
// the strip and shoe sizes, and the positions of the front- and rear-most LEDs.
// Becky's shoes: 39 LEDs total, 20 LEDs long, LED #5 at back.
// Phil's shoes: 43 LEDs total, 22 LEDs long, LED #6 at back.
#define N_LEDS 37 // TOTAL number of LEDs in strip
#define SHOE_LEN_LEDS 18 // Number of LEDs down ONE SIDE of shoe
#define SHOE_LED_BACK 5// Index of REAR-MOST LED on shoe
#define STEP_PIN A7 // Analog input for footstep
#define LED_PIN 9 // NeoPixel strip is connected here
#define MAXSTEPS 3 // Process (up to) this many concurrent steps

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int
  stepMag[MAXSTEPS], // Magnitude of steps
  stepX[MAXSTEPS], // Position of 'step wave' along strip
  mag[SHOE_LEN_LEDS], // Brightness buffer (one side of shoe)
  stepFiltered, // Current filtered pressure reading
  stepCount, // Number of 'frames' current step has lasted
  stepMin, // Minimum reading during current step
  stepTrigger, // Reading must be below this to trigger ste
  stepHysteresis, // After trigger, must return to this level
  multiplier; // Used to map the difference between the hysteresis threshold and the max pressure to an index in a 255 long array.
  
uint8_t
  stepNum = 0, // Current step number in stepMag/stepX tables
  dup[SHOE_LEN_LEDS], // Inside/outside copy indexes
  color0[3], // "darkest" color (use BLACK for fade-out)
  color1[3], // "second-darkest" color
  color2[3], // "second-brightest" color
  color3[3]; // "brightest" color

boolean
  stepping = false; // If set, step was triggered, waiting to release

// setMode() reads the current value of the mode counter and applies
// whatever colors you define for each mode. The mode that will be used
// next time you turn on the shoes is defined by the variable nextmode.
void setMode() {
  progmode currentmode = (progmode)EEPROM.read(MODEADDRESS); // read mode counter from EEPROM (non-volatile memory)
  progmode nextmode;
  switch(currentmode) {
    default:
    case MODE_FIRE:
      memcpy(color0, BLACK, 3);
      memcpy(color1, RED, 3);
      memcpy(color2, YELLOW, 3);
      memcpy(color3, WHITE, 3);
      nextmode = MODE_RGB;
      break;
    case MODE_RGB:
      memcpy(color0, BLACK, 3);
      memcpy(color1, RED, 3);
      memcpy(color2, GREEN, 3);
      memcpy(color3, BLUE, 3);
      nextmode = MODE_CMY;
      break;
    case MODE_CMY:
      memcpy(color0, BLACK, 3);
      memcpy(color1, CYAN, 3);
      memcpy(color2, MAGENTA, 3);
      memcpy(color3, YELLOW, 3);
      nextmode = MODE_BGY;
      break;
    case MODE_BGY:
      memcpy(color0, BLACK, 3);
      memcpy(color1, BLUE, 3);
      memcpy(color2, GREEN, 3);
      memcpy(color3, YELLOW, 3);
      nextmode = MODE_FIRE;
      break;
  }
  EEPROM.write(MODEADDRESS, (uint8_t)nextmode);   
}

// When you first start the program, all 4 colors for the current mode
// are displayed using this function before calibration starts.
void displayColors() {
  for (long i=0; i<N_LEDS; i++) {
    uint8_t r = rValue(i*25L);
    uint8_t g = gValue(i*25L);
    uint8_t b = bValue(i*25L);
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
}

void setAllToColor(uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0; i<N_LEDS; i++) {
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
}

void calibrate() {
  // Calculate the trigger and hysteresis values
  elapsedMillis timer;
  int count = 10;
  int maxes[count]; // Keep track of the biggest and smallest values observed
  int mins[count]; // Keep track of the biggest and smallest values observed
  for(int i=0; i<count; i++) {
    maxes[i] = 0;
    mins[i] = 1000;
  }
  displayColors();
  while (timer < 5000) { // Calibrate for 5 seconds
    stepFiltered = ((stepFiltered * 3) + analogRead(STEP_PIN)) >> 2;
    Serial.print("stepFitered calibration = " );
    Serial.println(stepFiltered);
    for(int i=0; i<count; i++) {
      if (stepFiltered > maxes[i]) { // If this value is bigger, add it to the array
        for(int j=count-1; j > i; j--) { // Push the lowest value out of the array
          maxes[j] = maxes[j-1];
        }
        maxes[i] = stepFiltered; // Add the new value
        continue;
      }
    }
    for(int i=0; i<count; i++) { // Same deal but for mins
      if (stepFiltered < mins[i]) {
        for(int j=count-1; j > i; j--) {
          mins[j] = mins[j-1];
        }
        mins[i] = stepFiltered;
        continue;
      }
    }
  }
  setAllToColor(0, 0, 0);
  stepTrigger = mins[count-1] + (maxes[count-1] - mins[count-1]) * 0.4; // The best values for your shoes may not be 0.4 and 0.5. 
  stepHysteresis = mins[count-1] + (maxes[count-1] - mins[count-1]) * 0.5; // It will depend on your specific sensor
  multiplier = 1800 / (stepHysteresis - mins[count-1]); // This magic value is chosen so there's a mix of all colors
  Serial.print("Trigger = ");
  Serial.println(stepTrigger);
  Serial.print("Hysteresis = ");
  Serial.println(stepHysteresis);
  Serial.print("Multiplier = ");
  Serial.println(multiplier);
}

void setup() {
  Serial.begin(9600);
  setMode();
  pinMode(9, INPUT_PULLUP); // Set internal pullup resistor for sensor pin
  // As previously mentioned, the step animation is mirrored on the inside and
  // outside faces of the shoe. To avoid a bunch of math and offsets later, the
  // 'dup' array indicates where each pixel on the outside face of the shoe should
  // be copied on the inside. (255 = don't copy, as on front- or rear-most LEDs).
  // Later, the colors for the outside face of the shoe are calculated and then get
  // copied to the appropriate positions on the inside face.
  memset(dup, 255, sizeof(dup));
  int8_t a, b;
  for(a=1 , b=SHOE_LED_BACK-1 ; b>=0 ;) dup[a++] = b--;
  for(a=SHOE_LEN_LEDS-2, b=SHOE_LED_BACK+SHOE_LEN_LEDS; b<N_LEDS;) dup[a--] = b++;
  // Clear step magnitude and position buffers
  memset(stepMag, 0, sizeof(stepMag));
  memset(stepX , 0, sizeof(stepX));
  strip.begin();
  stepFiltered = analogRead(STEP_PIN); // Initial input
  calibrate();
}

void loop() {
  uint8_t i, j;
  // Read analog input, with a little noise filtering
  stepFiltered = ((stepFiltered * 3) + analogRead(STEP_PIN)) >> 2;
  Serial.print("stepFitered = " );
  Serial.println(stepFiltered);
  // The strip doesn't simply display the current pressure reading. Instead,
  // there's a bit of an animated flourish from heel to toe. This takes time,
  // and during quick foot-tapping there could be multiple step animations
  // 'in flight,' so a short list is kept.
  if(stepping) { // If a step was previously triggered...
    if(stepFiltered >= stepHysteresis) { // Has step let up?
      stepping = false; // Yep! Stop monitoring.
      // Add new step to the step list (may be multiple in flight)
      stepMag[stepNum] = (stepHysteresis - stepMin) * multiplier; // Step intensity
      stepX[stepNum] = -80; // Position starts behind heel, moves forward
      if(++stepNum >= MAXSTEPS) stepNum = 0; // If many, overwrite oldest
    } else if(stepFiltered < stepMin) stepMin = stepFiltered; // Track min val
  } else if(stepFiltered < stepTrigger) { // No step yet; watch for trigger
    stepping = true; // Got one!
    stepMin = stepFiltered; // Note initial value
  }
  // Render a 'brightness map' for all steps in flight. It's like
  // a grayscale image; there's no color yet, just intensities.
  int mx1, px1, px2, m;
  memset(mag, 0, sizeof(mag)); // Clear magnitude buffer
  for(i=0; i<MAXSTEPS; i++) { // For each step...
    if(stepMag[i] <= 0) continue; // Skip if inactive
    for(j=0; j<SHOE_LEN_LEDS; j++) { // For each LED...
      // Each step has sort of a 'wave' that's part of the animation,
      // moving from heel to toe. The wave position has sub-pixel
      // resolution (4X), and is up to 80 units (20 pixels) long.
      mx1 = (j << 2) - stepX[i]; // Position of LED along wave
      if((mx1 <= 0) || (mx1 >= 80)) continue; // Out of range
      if(mx1 > 64) { // Rising edge of wave; ramp up fast (4 px)
        m = ((long)stepMag[i] * (long)(80 - mx1)) >> 4;
      } else { // Falling edge of wave; fade slow (16 px)
        m = ((long)stepMag[i] * (long)mx1) >> 6;
      }
      mag[j] += m; // Add magnitude to buffered sum
    }
    stepX[i]++; // Update position of step wave
    if(stepX[i] >= (80 + (SHOE_LEN_LEDS << 2)))
      stepMag[i] = 0; // Off end; disable step wave
    else
      stepMag[i] = ((long)stepMag[i] * 127L) >> 7; // Fade
  }
  // For a little visual interest, some 'sparkle' is added.
  // The cumulative step magnitude is added to one pixel at random.
  long sum = 0;
  for(i=0; i<MAXSTEPS; i++) sum += stepMag[i];
  if(sum > 0) {
    i = random(SHOE_LEN_LEDS);
    mag[i] += sum / 4;
  }
  // Now the grayscale magnitude buffer is remapped to color for the LEDs.
  // The code below uses a blackbody palette, which fades from white to yellow
  // to red to black. The goal here was specifically a "walking on fire"
  // aesthetic, so the usual ostentatious rainbow of hues seen in most LED
  // projects is purposefully skipped in favor of a more plain effect.
  uint8_t r, g, b;
  long level;
  for(i=0; i<SHOE_LEN_LEDS; i++) { // For each LED on one side...
    level = mag[i]; // Pixel magnitude (brightness)
    r = rValue(level);
    g = gValue(level);
    b = bValue(level);
    // Set R/G/B color along outside of shoe
    strip.setPixelColor(i+SHOE_LED_BACK, r, g, b);
    // Pixels along inside are funny...
    j = dup[i];
    if(j < 255) strip.setPixelColor(j, r, g, b);
  }
  strip.show();
  delayMicroseconds(3000);
}

// The following three functions do the math to smoothly change between any
// arbitrary set of colors depending on the "brightness" level.
// level = 0: color0
// level = 256: color1
// level = 512: color2
// level = 768+: color3

uint8_t rValue(long level) {
  uint8_t r;
  if(level < 256) {
    r = pgm_read_byte(&gamma[(color0[0]*(255L-level) + color1[0]*(level) + 128L)>>8]);
  } else if (level < 512) {
    r = pgm_read_byte(&gamma[(color1[0]*(255L-(level-256L)) + color2[0]*(level-256L) + 128L)>>8]);
  } else if (level < 768) {
    r = pgm_read_byte(&gamma[(color2[0]*(255L-(level-512L)) + color3[0]*(level-512L) + 128L)>>8]);
  } else {
    r = pgm_read_byte(&gamma[color3[0]]);
  }
  return r;
}

uint8_t gValue(long level) {
  uint8_t g;
  if(level < 256) {
    g = pgm_read_byte(&gamma[(color0[1]*(255L-level) + color1[1]*(level) + 128L)>>8]);
  } else if (level < 512) {
    g = pgm_read_byte(&gamma[(color1[1]*(255L-(level-256L)) + color2[1]*(level-256L) + 128L)>>8]);
  } else if (level < 768) {
    g = pgm_read_byte(&gamma[(color2[1]*(255L-(level-512L)) + color3[1]*(level-512L) + 128L)>>8]);
  } else {
    g = pgm_read_byte(&gamma[color3[1]]);
  }
  return g;
}

uint8_t bValue(long level) {
  uint8_t b;
  if(level < 256) {
    b = pgm_read_byte(&gamma[(color0[2]*(255L-level) + color1[2]*(level) + 128L)>>8]);
  } else if (level < 512) {
    b = pgm_read_byte(&gamma[(color1[2]*(255L-(level-256L)) + color2[2]*(level-256L) + 128L)>>8]);
  } else if (level < 768) {
    b = pgm_read_byte(&gamma[(color2[2]*(255L-(level-512L)) + color3[2]*(level-512L) + 128L)>>8]);
  } else {
    b = pgm_read_byte(&gamma[color3[2]]);
  }
  return b;
}
