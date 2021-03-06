/*
 ADSO - Arduino Digital Storage Oscilloscope
 v1.0 (c) 2017     www.github.com/zooxo/adso
 
 ADSO is a digital storage oscilloscope running on an arduino microcontroller.
 It samples, stores and analyses an electrical signal.
 
 Features:
 * Samples signal on one channel up to 500 Hz and 50 Volts
 * Shows signal graphically (80x64 pixel respective 10x8 divisions) as pixmap or polygon
 * Selectable scales (Volts per division, ms per division)
 * 1:1 and 10:1 probe
 * Triggering of periodic signals (selectable trigger level)
 * Selectable x and y offset
 * Reference signal (square, 0...5V)
 * Hold/save/load/reset signal and adjustments (permanent via EEPROM)
 * Fourier transformation (frequency analysis and harmonics)
 
*/

// INCLUDES & DEFINES

// Initialize OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
// Initialize EEEPROM
#include <EEPROM.h>
// Initialize custom keyboard (2x3)
#include <Keypad.h>
const byte ROWS = 2; //two rows
const byte COLS = 3; //four columns
char hexaKeys[ROWS][COLS] = { // "qweasd"
  {
    'q','w','e'}
  ,{
    'a','s','d'}
};
byte rowPins[ROWS] = {
  2,4}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {
  6,5,3}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(hexaKeys),rowPins,colPins,ROWS,COLS); 

// defines
#define HALFY 31 // y-coordinate for zero line
#define DEFAULTSAMPLETIME 625 // default sample time in us
#define SAMPLESIZE 160 // number of stored values
#define SAMPLEDRAW 80 // number of drawn values
#define SAMPLESPERDISPLAY 200 // display after 200 samples
#define DEFAULTSIGNALRATE 10000 // rate of square reference signal in us
#define MAXMENUNR 6 // number of menus
#define DEFAULTTRIGLVL 25 // default trigger level
#define MAXTRIGLVL 63 // maximal trigger level
#define MINTRIGLVL 0 // minimal trigger level
#define TRIGLVLSTEP 4 // step change of triglevel
#define XTEXT 81 // x-coordinate where text starts
#define LETTERWIDTHBIG 12
#define LETTERWIDTHSMALL 6
#define XOFFSETMIN -10 // extrema for offsets
#define XOFFSETMAX 30
#define YOFFSETMIN -32
#define YOFFSETMAX 64
#define EEPROMOFFSET 64 // eeprom adress where sample is stored
#define FCNR 10 //number of fourier coefficients

// define flags and set initial values
boolean istriglevel=false; // no triggering
boolean isdrawlines=false; // no line drawing
boolean isreference=true; // reference signal (signalpin) on
boolean isground=false; // no grounding of signal
boolean ishold=false; // signal holded
boolean isfourier=false; // fourier analysis

// variables
byte pin=1,pwmpin=11,signalpin=9; // analog read pin, PWM- and square-reference pin
char key; // scanned key
byte menunr=1; // default menu number
byte val[SAMPLESIZE],ptr=0; // sampled value storage
char xoffset=0,yoffset=0; // offset variables
byte triglevel=DEFAULTTRIGLVL; // trigger level
byte displaycounter=0; // counts scans and display every SCANSPERDISPLAY times
double a[FCNR]; // fourier coefficients
byte nstrip; // number of sample values used for fourier
unsigned long scanstart=0; // used for square reference 
unsigned long signaltime=DEFAULTSIGNALRATE,signalmicros=0;

// amplitude settings
int voltsteps[]={
  100,200,500,1000,2000,5000}; // im mV
#define numberofvoltsteps (sizeof(voltsteps)/sizeof(const int *))
byte vsnr=4; // volt step index (default 4 = 2000 mV/div)

// time settings
int sampletimes[]={
  125,250,625,1250,2500}; // in ms
#define numberofsampletimes (sizeof(sampletimes)/sizeof(const int *))
byte stnr=2; // sample time index (default 2 = 625 ms/div)
unsigned long timestamp=0,startmicros=0; // used for sample time measurement
unsigned long sampletime=sampletimes[stnr],sampledtime=0;


// SUBPROGRAMS

void curpos(byte x, byte y, byte font) { // set cursor position and font size
  display.setCursor(x,y);
  display.setTextSize(font);
}

void fourier() { // calculate fourier coefficients
  byte trigpnt1=0,trigpnt2=0;
  for(byte i=0;i<SAMPLESIZE-1;i++) { // find 1st trigpoint
    if((val[i]>=triglevel)&&(val[i+1]<triglevel)) {
      trigpnt1=i;
      for(byte j=i+1;j<SAMPLESIZE-1;j++) { // find 2nd trigpoint
        if((val[j]>=triglevel)&&(val[j+1]<triglevel)) {
          trigpnt2=j;
          break;
        }
      }
      break;
    }
  }
  nstrip=trigpnt2-trigpnt1;
  for(byte j=0;j<FCNR;j++) { // calculate fourier coefficients
    a[j]=0;
    for(byte k=trigpnt1;k<trigpnt2;k++) { // cosine
      a[j]+=(HALFY-val[k])*cos(j*(k-trigpnt1)*2*PI/nstrip);
    }
    for(byte k=trigpnt1;k<trigpnt2;k++) { // sine
      a[j]+=(HALFY-val[k])*sin(j*(k-trigpnt1)*2*PI/nstrip);
    }
    a[j]=a[j]/nstrip*2;
    if(j==0) a[j]/=2;
    a[j]=HALFY-a[j];
  }
}

void printoled() { // print display
  byte trigptr=0;
  display.clearDisplay();
  if(isfourier) { // call fourier and draw x-axis labels
    fourier();
    for(byte i=0;i<FCNR;i++) {
      curpos(1+i*8,38,1);
      display.print(i);
    }
  }
  display.drawLine(XTEXT-2,46,127,46,WHITE); // draw separation lines 
  display.drawLine(XTEXT-2,10,127,10,WHITE);
  display.drawLine(XTEXT-2,0,XTEXT-2,63,WHITE);
  display.drawLine(120,47,120,63,WHITE);
  display.drawPixel(39,0,WHITE); //draw grid
  display.drawPixel(39,7,WHITE);
  display.drawPixel(39,15,WHITE);
  display.drawPixel(39,23,WHITE);
  display.drawPixel(39,HALFY,WHITE);
  if(!isfourier) {
    display.drawPixel(39,39,WHITE);
    display.drawPixel(39,47,WHITE);
    display.drawPixel(39,55,WHITE);
    display.drawPixel(39,63,WHITE);
  }
  display.drawPixel(0,HALFY,WHITE);
  display.drawPixel(7,HALFY,WHITE);
  display.drawPixel(15,HALFY,WHITE);
  display.drawPixel(23,HALFY,WHITE);
  display.drawPixel(31,HALFY,WHITE);
  display.drawPixel(47,HALFY,WHITE);
  display.drawPixel(55,HALFY,WHITE);
  display.drawPixel(63,HALFY,WHITE);
  display.drawPixel(71,HALFY,WHITE);
  if(isfourier) { // draw frequency spectrum
    for(byte i=0;i<FCNR;i++) {
      display.drawLine(i*8,a[i],(i+1)*8,a[i],WHITE);
    }
  }
  else if(istriglevel) {
    display.drawPixel(127,triglevel,WHITE); // draw triglevel indicator
    display.drawPixel(126,triglevel,WHITE);
    display.drawPixel(127,triglevel+1,WHITE);
    display.drawPixel(127,triglevel-1,WHITE);
    for(byte i=0;i<SAMPLEDRAW-1;i++) { // find trigpoint
      if((val[i]>=triglevel)&&(triglevel>val[i+1])) {
        trigptr=i;
        break;
      }
    }
    for(byte i=0;i<SAMPLEDRAW;i++) { //draw values triggered
      if(isdrawlines) display.drawLine(i+xoffset,val[trigptr],i+1+xoffset,val[trigptr+1],WHITE);
      else display.drawPixel(i+xoffset,val[trigptr],WHITE);
      trigptr++;
    }
  }
  else {
    for(byte i=0;i<SAMPLEDRAW;i++) { //draw values untriggered
      if(isdrawlines) display.drawLine(i,val[i],i+1,val[i+1],WHITE);
      else display.drawPixel(i,val[i],WHITE);
    }
  }

  curpos(XTEXT,13,2); // amplitude (V/div)
  if(voltsteps[vsnr]<1000) display.print((double)(voltsteps[vsnr]/1000.0),1);
  else display.print(voltsteps[vsnr]/1000);
  curpos(XTEXT+3*LETTERWIDTHBIG,13,1);
  display.print("V");

  curpos(XTEXT,30,2); // frequency or time (Hz or ms/div)
  if(isfourier) {
    display.print((int)(1/(nstrip*sampledtime/1000000.0)));
    curpos(XTEXT+2*LETTERWIDTHBIG+LETTERWIDTHSMALL,30,1);
    display.print("Hz");
  }
  else {
    display.print((int)((double)(sampledtime*8.0/1000)+0.5));
    curpos(XTEXT+2*LETTERWIDTHBIG+LETTERWIDTHSMALL,30,1);
    display.print("ms");
  }

  curpos(122,49,1); // menu
  display.print("+");
  curpos(122,57,1);
  display.print("-");
  switch(menunr) {
  case 1:
    curpos(XTEXT,49,2);
    display.print("V");
    curpos(XTEXT+LETTERWIDTHBIG+LETTERWIDTHSMALL,49,2);
    display.print("t");
    break;
  case 2:
    curpos(XTEXT,49,2);
    display.print("T");
    curpos(XTEXT+LETTERWIDTHBIG+LETTERWIDTHSMALL,49,1);
    display.print("ON");
    curpos(XTEXT+LETTERWIDTHBIG+LETTERWIDTHSMALL,57,1);
    display.print("OFF");
    break;
  case 3:
    curpos(XTEXT,49,2);
    display.print("X");
    curpos(XTEXT+LETTERWIDTHBIG+LETTERWIDTHSMALL,49,1);
    display.print("RST");
    break;
  case 4:
    curpos(XTEXT,49,2);
    display.print("Y");
    curpos(XTEXT+LETTERWIDTHBIG+LETTERWIDTHSMALL,49,1);
    display.print("RST");
    break;
  case 5:
    curpos(XTEXT,49,1);
    display.print("PIX");
    curpos(XTEXT+3*LETTERWIDTHSMALL+3,49,1);
    display.print("REF");
    curpos(XTEXT,57,1);
    display.print("GND");
    curpos(XTEXT+3*LETTERWIDTHSMALL+3,57,1);
    display.print("FFT");
    break;
  case 6:
    curpos(XTEXT,49,1);
    display.print("HLD");
    curpos(XTEXT+3*LETTERWIDTHSMALL+3,49,1);
    display.print("SAV");
    curpos(XTEXT,57,1);
    display.print("RST");
    curpos(XTEXT+3*LETTERWIDTHSMALL+3,57,1);
    display.print("LD");
    break;
  default:
    break;
  }

  // indicators
  if(istriglevel) { // trigger
    curpos(XTEXT,0,1);
    display.print("T");
  }
  if(isdrawlines) { // draw lines instead of pixels
    curpos(XTEXT+LETTERWIDTHSMALL,0,1);
    display.print("L");
  }
  if(isfourier) { // fourier
    curpos(XTEXT+2*LETTERWIDTHSMALL,0,1);
    display.print("F");
  }
  if(ishold) { // hold
    curpos(XTEXT+3*LETTERWIDTHSMALL,0,1);
    display.print("H");
  }
  if(xoffset||yoffset) { // x or y offset
    curpos(XTEXT+4*LETTERWIDTHSMALL,0,1);
    display.print("O");
  }
  if(isreference) { // reference signal on
    curpos(XTEXT+5*LETTERWIDTHSMALL,0,1);
    display.print("R");
  }
  if(isground) { // signal grounded
    curpos(XTEXT+6*LETTERWIDTHSMALL,0,1);
    display.print("G");
  }
  if(sampledtime>sampletimes[stnr]*1.02) { // sample time error
    curpos(XTEXT+7*LETTERWIDTHSMALL,0,1);
    display.print("S");
  }

  display.display();
}

void reset() { // reset adjustments to default values
  istriglevel=false;
  isdrawlines=false;
  isreference=false;
  isground=false;
  ishold=false;
  isfourier=false;
  triglevel=DEFAULTTRIGLVL;
  xoffset=yoffset=0;
  vsnr=4;
  stnr=2;
  startmicros=micros();
  for(byte i=0;i<SAMPLESIZE;i++) val[i]=0;
}

void save() { // saves adjustments and sample values
  byte i=0;
  EEPROM.write(i++,istriglevel);
  EEPROM.write(i++,isdrawlines);
  EEPROM.write(i++,isreference);
  EEPROM.write(i++,isground);
  EEPROM.write(i++,ishold);
  EEPROM.write(i++,isfourier);
  EEPROM.write(i++,triglevel);
  EEPROM.write(i++,xoffset);
  EEPROM.write(i++,yoffset);
  EEPROM.write(i++,vsnr);
  EEPROM.write(i++,stnr);
  for(i=0;i<SAMPLESIZE;i++) EEPROM.write(i+EEPROMOFFSET,val[i]);
}

void load() { // loads adjustments and sample values
  byte i=0;
  istriglevel=EEPROM.read(i++);
  isdrawlines=EEPROM.read(i++);
  isreference=EEPROM.read(i++);
  isground=EEPROM.read(i++);
  ishold=EEPROM.read(i++);
  isfourier=EEPROM.read(i++);
  triglevel=EEPROM.read(i++);
  xoffset=EEPROM.read(i++);
  yoffset=EEPROM.read(i++);
  vsnr=EEPROM.read(i++);
  stnr=EEPROM.read(i++);
  ishold=true;
  for(byte i=0;i<SAMPLESIZE;i++) val[i]=EEPROM.read(i+EEPROMOFFSET);
}


// SETUP & LOOP

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c); // initialize display
  display.setTextColor(WHITE);
  display.setTextWrap(false);

  pinMode(signalpin,OUTPUT); // initialize square reference pin
  digitalWrite(signalpin,LOW);

  pinMode(pwmpin,OUTPUT); // initialize PWM reference pin
  analogWrite(pwmpin,64); // value 1:4 (high:low)

  reset();

  display.clearDisplay(); // Welcome message
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.println(F(" ADSO 1.0"));
  display.println();
  display.println();
  display.println(F(" WELCOME"));
  display.display();
  delay(750);
}

void loop() {
  timestamp=micros();
  if(timestamp-startmicros>=sampletime) { // measurement if desired sampletime is reached
    if(!ishold)
      if(isground) val[ptr]=HALFY;
      else
        val[ptr]=(yoffset+HALFY+40000/voltsteps[vsnr]-40000.0/511/voltsteps[vsnr]*analogRead(pin));
    // val = k * x + d
    // k   = 8pxperdiv / Vpd / ((1024analogvals - 512valforzero) / (10xdiv / 2ranges))
    // d   = 32ypixforzero - k * 512xpixforzero
    ptr++;
    if(ptr>=SAMPLESIZE) { // one diagram sampled (scan)
      ptr=0;
      sampledtime=(timestamp-scanstart)/SAMPLESIZE; // calibrate sampletime
      sampletime=sampletime*sampletimes[stnr]/sampledtime;
      displaycounter++;
      if(displaycounter>=SAMPLESPERDISPLAY/sampletimes[stnr]) { // print screen
        printoled();
        displaycounter=0;
      }
      scanstart=micros();
    }
    startmicros=micros();
  }

  if(isreference) { // square reference signal at signalpin
    if((timestamp-signalmicros)>=signaltime) {
      if(digitalRead(signalpin)) digitalWrite(signalpin,LOW);
      else digitalWrite(signalpin,HIGH);
      signalmicros=micros();
    }
  }

  key=keypad.getKey();
  if(key!=NO_KEY) { // process if character input occurs
    if(key=='e') // next or previous menu
      if(menunr<MAXMENUNR) menunr++;
      else menunr=1;
    if(key=='d')
      if(menunr>1) menunr--;
      else menunr=MAXMENUNR;

    if(menunr==1) { // menu1
      switch(key) {
      case 'q': // V+
        if(vsnr<numberofvoltsteps-1) vsnr++;
        break;
      case 'a': // V-
        if(vsnr>0) vsnr--;
        break;
      case 'w': // t+
        if(stnr<numberofsampletimes-1) stnr++;
        sampletime=sampletimes[stnr];
        break;
      case 's': // t-
        if(stnr>0) stnr--;
        sampletime=sampletimes[stnr];
        break;
      }
    }
    if(menunr==2) { // menu2
      switch(key) {
      case 'q': // triggerlevel up
        if(triglevel>MINTRIGLVL+TRIGLVLSTEP) triglevel-=TRIGLVLSTEP;
        break;
      case 'a': // triggerlevel down
        if(triglevel<MAXTRIGLVL-TRIGLVLSTEP) triglevel+=TRIGLVLSTEP;
        break;
      case 'w': // trigger on
        istriglevel=true;
        break;
      case 's': // trigger off
        istriglevel=false;
        break;
      }
    }
    if(menunr==3) { // menu3
      switch(key) {
      case 'q': //
        if(xoffset<XOFFSETMAX) xoffset++;
        break;
      case 'a': //
        if(xoffset>XOFFSETMIN) xoffset--;
        break;
      case 'w': //
        xoffset=0;
        break;
      case 's': //
        break;
      }
    }
    if(menunr==4) { // menu4
      switch(key) {
      case 'q': //
        if(yoffset>YOFFSETMIN) yoffset--;
        break;
      case 'a': //
        if(yoffset<YOFFSETMAX) yoffset++;
        break;
      case 'w': //
        yoffset=0;
        break;
      case 's': //
        break;
      }
    }
    if(menunr==5) { // menu5
      switch(key) {
      case 'q': // toggle lines/pixel
        if(isdrawlines) isdrawlines=false;
        else isdrawlines=true;
        break;
      case 'a': // toggle grounding (a holded signal can not be grounded)
          if(isground) isground=false;
          else if(!ishold) isground=true;
        break;
      case 'w': // toggle reference signal
        if(isreference) { 
          isreference=false;
          digitalWrite(signalpin,LOW);
        }
        else isreference=true;
        break;
      case 's': // toggle FFT
        if(isfourier) isfourier=false;
        else isfourier=true;
        break;
      }
    }
    if(menunr==6) { // menu6
      switch(key) {
      case 'q': // toggle hold
        if(ishold) ishold=false;
        else ishold=true;
        break;
      case 'a': // reset
        reset();
        break;
      case 'w': // save
        save();
        break;
      case 's': // load
        load();
        break;
      }
    }
  }

}

