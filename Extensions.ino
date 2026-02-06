/*
  Incubator Extension - Version 1.0 - Sep 2025
  Hartmut Grawe - github.com/ersatzmoco - Sep 2025

  See http://www.ulisp.com/show?4GMV

  Licensed under the MIT license: https://opensource.org/licenses/MIT
*/

#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#include <U8g2lib.h>

#include <Servo.h>

Adafruit_BME680 bme(&Wire); // I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled = U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

struct ulservo {
  int snum;
  int pin;
  struct ulservo* nextservo;
  Servo servo;
};

struct ulservo* servolist = NULL;
struct ulservo* curservo = NULL;
    
/*
  (load-mono fname arr [offx] [offy])
  Open monochrome BMP file fname from SD if it exits and copy it into the two-dimensional uLisp bit array provided.
  When the image is larger than the array, only the upper leftmost area of the bitmap fitting into the array is loaded.
  Providing offx and offy you may move the "window" of the array to other parts of the bitmap (useful e.g. for tiling).
*/
object *fn_LoadMono (object *args, object *env) {

  SD.begin(SDCARD_SS_PIN);

  int slength = stringlength(checkstring(first(args)))+1;
  char* fnbuf = (char*)malloc(slength);
  cstring(first(args), fnbuf, slength);
  File file;

  if (!SD.exists(fnbuf)) {
    pfstring("File not found", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }
  object* array = second(args);
  if (!arrayp(array)) {
    pfstring("Argument is not an array", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }
  object* dimensions = cddr(array);
  if (listp(dimensions)) {
    if (listlength(dimensions) != 2) {
      pfstring("Array must be two-dimensional", (pfun_t)pserial);
      free(fnbuf);
      return nil;
    }
  }
  else {
    pfstring("Array must be two-dimensional", (pfun_t)pserial);
      free(fnbuf);
      return nil;
  }
  args = cddr(args);
  int offx = 0;
  int offy = 0;

  if (args != NULL) {
    offx = checkinteger(car(args));
    args = cdr(args);
    if (args != NULL) {
      offy = checkinteger(car(args));
    }
  }
  (void) args;

  int aw = abs(first(dimensions)->integer);
  int ah = abs(second(dimensions)->integer);
  int bit;
  object* subscripts;
  object* ox;
  object* oy;
  object* oyy;
  object** element;

  char buffer[BUFFERSIZE];
  file = SD.open(fnbuf);
  if (!file) { 
    pfstring("Problem reading from SD card", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  char b = file.read();
  char m = file.read();
  if ((m != 77) || (b != 66)) {
    pfstring("No BMP file", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  file.seek(10);
  uint32_t offset = SDRead32(file);
  SDRead32(file);
  int32_t width = SDRead32(file);
  int32_t height = SDRead32(file);
  int linebytes = floor(width / 8);
  int restbits = width % 8;
  if (restbits > 0) linebytes++;
  int zpad = 0;
  if ((linebytes % 4) > 0) {
    zpad = (4 - (linebytes % 4));
  }

  file.seek(28);
  uint16_t depth = file.read();
  if (depth > 1) { 
    pfstring("No monochrome bitmap file", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  file.seek(offset);

  int lx = 0;
  int bmpbyte = 0;
  int bmpbit = 0;

  for (int ly = (height - 1); ly >= 0; ly--) {
    for (int bx = 0; bx < linebytes; bx++) {
      bmpbyte = file.read();
      for (int bix = 0; bix < 8; bix++) {
        lx = (bx * 8) + bix;
        if ((lx < (aw+offx)) && (ly < (ah+offy)) && (lx >= offx) && (ly >= offy)) {
          ox = number(lx-offx);
          oy = number(ly-offy);
          oyy = cons(oy, NULL);
          subscripts = cons(ox, oyy);
          element = getarray(array, subscripts, env, &bit);

          bmpbit = bmpbyte & (1 << (7-bix));
          if (bmpbit > 0) {
            bmpbit = 1;
          }
          else {
            bmpbit = 0;
          }
          *element = number((checkinteger(*element) & ~(1<<bit)) | bmpbit<<bit);

          myfree(subscripts);
          myfree(oyy);
          myfree(oy);
          myfree(ox);
        }
      }
    }
    //ignore trailing zero bytes
    if (zpad > 0) {
      for (int i = 0; i < zpad; i++) {
        file.read();
      }
    }
  }

  file.close();
  free(fnbuf);
  return nil;
}


//
// OLED graphics and text routines - in part a modified copy of GFX routines in uLisp core
// No stream support to avoid major modification of uLisp core 
//

/*
  (oled-begin [adr])
  Initialize OLED (optionally using I2C address adr, otherwise using default address #x3C (7 bit)/#x78 (8 bit)).
*/
object *fn_OledBegin (object *args, object *env) {
  (void) env;

  uint8_t adr = 0x78;
  if (args != NULL) {
    adr = (uint8_t)checkinteger(first(args));
  }

  oled.setI2CAddress(adr);
  if (!oled.begin()) {
    Serial.println("OLED Not Found!");
    return nil;
  }
  oled.clearBuffer();
  oled.setFont(u8g2_font_scrum_te);
  return tee;
}  

/*
  (oled-clear)
  Clear OLED.
*/
object *fn_OledClear (object *args, object *env) {
  (void) args, (void) env;

  oled.clearBuffer();
  oled.sendBuffer();
  return nil;
}

/*
  (oled-set-rotation rot)
  Set rotation of screen. 0 = no rotation, 1 = 90 degrees, 2 = 180 degrees, 3 = 270 degrees, 4 = hor. mirrored, 5 = vert. mirrored.
*/
object *fn_OledSetRotation (object *args, object *env) {
  (void) env;
  uint8_t rot = (uint8_t)checkinteger(first(args));
  if (rot > 5) rot = 5;

  switch (rot) {
    case 0:
        oled.setDisplayRotation(U8G2_R0);
    case 1:
        oled.setDisplayRotation(U8G2_R1);
    case 2:
        oled.setDisplayRotation(U8G2_R2);
    case 3:
        oled.setDisplayRotation(U8G2_R3);
    case 4:
        oled.setDisplayRotation(U8G2_MIRROR);
    case 5:
        oled.setDisplayRotation(U8G2_MIRROR_VERTICAL);    
  }
  
  oled.sendBuffer();
  return nil;
}

/*
  (oled-set-color fg)
  Set foreground color for text and graphics. Colors are 0 (clear/black), 1 (set/white) and 2 (XOR).
*/
object *fn_OledSetColor (object *args, object *env) {
  (void) env;

  uint8_t col = (uint8_t)checkinteger(first(args));
  if (col > 2) col = 2;

  oled.setDrawColor(col);
  return nil;
}

/*
  (oled-set-font n)
  Set text font number n. Values from 0 to 2 correspond to U8G2 font pixel
  heights 8, 16 und 32 (font families scrum (default), inb16 and logisoso32).
*/
object *fn_OledSetFont (object *args, object *env) {
  (void) env;

  int f = checkinteger(first(args));

  switch (f)
  {
    case 0:
      oled.setFont(u8g2_font_scrum_te);
      break;

    case 1:
      oled.setFont(u8g2_font_inb16_mf);
      break;

    case 2:
      oled.setFont(u8g2_font_logisoso32_tf);
      break;

    default:
      oled.setFont(u8g2_font_scrum_te);
  }

  return nil;
}

/*
  (oled-write-char x y c)
  Write char c to screen at location x y.
*/
object *fn_OledWriteChar (object *args, object *env) {
  (void) env;

  oled.drawGlyph(checkinteger(first(args)), checkinteger(second(args)), checkchar(third(args)));
  oled.sendBuffer();
  return nil;
}

/*
  (oled-write-string x y str)
  Write string str to screen at location x y.
*/
object *fn_OledWriteString (object *args, object *env) {
  (void) env;

  int slength = stringlength(checkstring(third(args)))+1;
  char *oledbuf = (char*)malloc(slength);
  cstring(third(args), oledbuf, slength);
  oled.drawStr(checkinteger(first(args)), checkinteger(second(args)), oledbuf);
  oled.sendBuffer();
  free(oledbuf);
  return nil;
}

/*
  (oled-draw-pixel x y)
  Draw pixel at position x y (using color set before).
*/
object *fn_OledDrawPixel (object *args, object *env) {
  (void) env;
  oled.drawPixel(checkinteger(first(args)), checkinteger(second(args)));
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-line x0 y0 x1 y1)
  Draw a line between positions x0/y0 and x1/y1.
*/
object *fn_OledDrawLine (object *args, object *env) {
  (void) env;
  uint16_t params[4];
  for (int i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawLine(params[0], params[1], params[2], params[3]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-hline x y w)
  Draw fast horizontal line at position X Y with length w.
*/
object *fn_OledDrawHLine (object *args, object *env) {
  (void) env;
  uint16_t params[3];
  for (int i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawHLine(params[0], params[1], params[2]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-vline x y h)
  Draw fast vertical line at position X Y with length h.
*/
object *fn_OledDrawVLine (object *args, object *env) {
  (void) env;
  uint16_t params[3];
  for (int i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawVLine(params[0], params[1], params[2]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-rect x y w h)
  Draw empty rectangle at x y with width w and height h.
*/
object *fn_OledDrawRect (object *args, object *env) {
  (void) env;
  uint16_t params[4];
  for (int i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawFrame(params[0], params[1], params[2], params[3]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-fill-rect x y w h)
  Draw filled rectangle at x y with width w and height h.
*/
object *fn_OledFillRect (object *args, object *env) {
  (void) env;
  uint16_t params[4];
  for (int i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawBox(params[0], params[1], params[2], params[3]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-circle x y r)
  Draw empty circle at position x y with radius r.
*/
object *fn_OledDrawCircle (object *args, object *env) {
  (void) env;
  uint16_t params[3];
  for (int i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawCircle(params[0], params[1], params[2]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-fill-circle x y r)
  Draw filled circle at position x y with radius r.
*/
object *fn_OledFillCircle (object *args, object *env) {
  (void) env;
  uint16_t params[3];
  for (int i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawDisc(params[0], params[1], params[2]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-draw-round-rect x y w h r)
  Draw empty rectangle at x y with width w and height h. Edges are rounded with radius r.
*/
object *fn_OledDrawRoundRect (object *args, object *env) {
  (void) env;
  uint16_t params[5];
  for (int i=0; i<5; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawRFrame(params[0], params[1], params[2], params[3], params[4]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-fill-round-rect)
  Draw filled rectangle at x y with width w and height h. Edges are rounded with radius r.
*/
object *fn_OledFillRoundRect (object *args, object *env) {
  (void) env;
  uint16_t params[5];
  for (int i=0; i<5; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawRBox(params[0], params[1], params[2], params[3], params[4]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-fill-triangle x0 y0 x1 y1 x2 y2)
  Draw filled triangle with corners at x0/y0, x1/y1 and x2/y2.
*/
object *fn_OledFillTriangle (object *args, object *env) {
  (void) env;
  uint16_t params[6];
  for (int i=0; i<6; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  oled.drawTriangle(params[0], params[1], params[2], params[3], params[4], params[5]);
  oled.sendBuffer();
  return nil;
}

/*
  (oled-display-bmp fname x y)
  Open monochrome BMP file fname from SD if it exits and display it on screen at position x y (using the color set before).
*/
object *fn_OledDisplayBMP (object *args, object *env) {
  (void) env;

  SD.begin(SDCARD_SS_PIN);

  int slength = stringlength(checkstring(first(args)))+1;
  char *fnbuf = (char*)malloc(slength);
  cstring(first(args), fnbuf, slength);
  File file;

  if (!SD.exists(fnbuf)) {
    pfstring("File not found", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }
  int x = checkinteger(second(args));
  int y = checkinteger(third(args));
  (void) args;

  char buffer[BUFFERSIZE];
  file = SD.open(fnbuf);
  if (!file) { 
    pfstring("Problem reading from SD card", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  char b = file.read();
  char m = file.read();
  if ((m != 77) || (b != 66)) {
    pfstring("No BMP file", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  file.seek(10);
  uint32_t offset = SDRead32(file);
  SDRead32(file);
  int32_t width = SDRead32(file);
  int32_t height = SDRead32(file);
  int linebytes = floor(width / 8);
  int restbits = width % 8;
  if (restbits > 0) linebytes++;
  int zpad = 0;
  if ((linebytes % 4) > 0) {
    zpad = (4 - (linebytes % 4));
  }

  file.seek(28);
  uint16_t depth = file.read();
  if (depth > 1) { 
    pfstring("No monochrome bitmap file", (pfun_t)pserial);
    free(fnbuf);
    return nil;
  }

  file.seek(offset);

  int lx = 0;
  int bmpbyte = 0;
  int bmpbit = 0;

  for (int ly = (height - 1); ly >= 0; ly--) {
    for (int bx = 0; bx < linebytes; bx++) {
      bmpbyte = file.read();
      for (int bix = 0; bix < 8; bix++) {
        lx = (bx * 8) + bix;
        bmpbit = bmpbyte & (1 << (7-bix));
        if (bmpbit > 0) {
          oled.drawPixel(x+lx, y+ly);
        }
      }
    }
    //ignore trailing zero bytes
    if (zpad > 0) {
      for (int i = 0; i < zpad; i++) {
        file.read();
      }
    }
  }

  oled.sendBuffer();
  file.close();
  free(fnbuf);
  return nil;
}

/*
  (oled-show-bmp arr x y)
  Show bitmap image contained in uLisp array arr on screen at position x y (using color set before).
*/
object *fn_OledShowBMP (object *args, object *env) {

  object* array = first(args);
  if (!arrayp(array)) error2("argument is not an array");

  object *dimensions = cddr(array);
  if (listp(dimensions)) {
    if (listlength(dimensions) != 2) error2("array must be two-dimensional");
  }
  else error2("array must be two-dimensional");

  int x = checkinteger(second(args));
  int y = checkinteger(third(args));
  (void) args;

  int aw = abs(first(dimensions)->integer);
  int ah = abs(second(dimensions)->integer);
  int bit;
  object* subscripts;
  object* ox;
  object* oy;
  object* oyy;
  object** element;

  int bmpbit = 0;
  for (int ay = 0; ay < ah; ay++) {
    for (int ax = 0; ax < aw; ax++) {
      ox = number(ax);
      oy = number(ay);
      oyy = cons(oy, NULL);
      subscripts = cons(ox, oyy);
      element = getarray(array, subscripts, env, &bit);
      if (bit < 0) {
        error2("OLED draws monochrome image only");
      }
      else {
        bmpbit = abs(checkinteger(*element) & (1<<bit));
        if (bmpbit > 0) {
          oled.drawPixel(x+ax, y+ay);
        }
      }
      myfree(subscripts);
      myfree(oyy);
      myfree(oy);
      myfree(ox);
    }
  }
  oled.sendBuffer();

  return nil;
}


//
// BME680 sensor routines
//

/*
  (bme-begin [adr])
  Initialize sensor (optionally using I2C address adr, otherwise using default address #x77).
*/
object *fn_BMEBegin (object *args, object *env) {
  (void) env;

  uint8_t adr = 0x77;
  if (args != NULL) {
    adr = (uint8_t)checkinteger(first(args));
  }

  if (!bme.begin(adr)) {
    Serial.println("Sensor Not Found!");
    return nil;
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  return tee;
}

/*
  (bme-read-temp)
  Read temperature (degree celsius).
*/
object *fn_BMEReadTemp (object *args, object *env) {
  (void) args, (void) env;

  object* temp = makefloat(bme.readTemperature());

  return temp;
}

/*
  (bme-read-hum)
  Read humidity (percent).
*/
object *fn_BMEReadHum (object *args, object *env) {
  (void) args, (void) env;

  object* temp = makefloat(bme.readHumidity());

  return temp;
}


//
// servo routines
//
/*
  (servo-attach snum pin [pwmin pwmax])
  Attach servo snum to pin. Optionally define new pulse width min/max in microseconds.
*/
object *fn_ServoAttach (object *args, object *env) {
  (void) env;
 
  int snum = checkinteger(first(args));   // zero based index!;
  int pin = checkinteger(second(args));
  int usmin = 544;
  int usmax = 2400;

  args = cdr(args);
  args = cdr(args);
  if (args != NULL) {
    usmin = checkinteger(first(args));
    args = cdr(args);
    if (args != NULL) {
      usmax = checkinteger(first(args));
    }
  }

  if(servolist == NULL) {
    if((servolist = (struct ulservo*)malloc(sizeof(struct ulservo))) == NULL) {
       pfstring("Out of memory", (pfun_t)pserial);
       return nil;
    }

    servolist->snum = snum;
    servolist->pin = pin;
    servolist->nextservo = NULL;
    servolist->servo = Servo();
    servolist->servo.attach(pin, usmin, usmax);
  }
  else {
    curservo = servolist;
    while(curservo->nextservo != NULL) {
      if ((curservo->snum == snum) || (curservo->pin == pin)) {
        pfstring("Servo number or pin already in use!", (pfun_t)pserial);
        return nil;
      }
      curservo = curservo->nextservo;
    }

    if ((curservo->snum == snum) || (curservo->pin == pin)) {
      pfstring("Servo number or pin already in use!", (pfun_t)pserial);
      return nil;
    }

    if((curservo->nextservo = (struct ulservo*)malloc(sizeof(struct ulservo))) == NULL) {
        pfstring("Out of memory", (pfun_t)pserial);
        return nil;
    }

    curservo = curservo->nextservo;

    curservo->snum = snum;
    curservo->pin = pin;
    curservo->servo = Servo();
    curservo->servo.attach(pin, usmin, usmax);
    curservo->nextservo = NULL;
  }
  return tee;
}

/*
  (servo-write)
  Set angle of servo snum in degrees (0 to 180).
*/
object *fn_ServoWrite (object *args, object *env) {
  (void) env;
 
  int snum = checkinteger(first(args));
  int angle = checkinteger(second(args));
  curservo = servolist;

  if (curservo != NULL) {

    while(curservo->snum != snum) {
      curservo = curservo->nextservo;
      if (curservo == NULL) break;
    }

    if(curservo != NULL) {
      curservo->servo.write(angle);
      return number(angle);
    }
  }

  pfstring("Servo not found", (pfun_t)pserial);
  return nil;
}

/*
  (servo-write-microseconds)
  Set angle of servo snum using a pulse width value in microseconds.
*/
object *fn_ServoWriteMicroseconds (object *args, object *env) {
  (void) env;
 
  int snum = checkinteger(first(args));
  int us = checkinteger(second(args));
  curservo = servolist;

  if (curservo != NULL) {

    while(curservo->snum != snum) {
      curservo = curservo->nextservo;
      if (curservo == NULL) break;
    }

    if(curservo != NULL) {
      curservo->servo.writeMicroseconds(us);
      return number(us);
    }
  }

  pfstring("Servo not found", (pfun_t)pserial);
  return nil;
}

/*
  (servo-read)
  Read current angle of servo snum in degrees.
*/
object *fn_ServoRead (object *args, object *env) {
  (void) env;

  int snum = checkinteger(first(args));
  curservo = servolist;

  if (curservo != NULL) {

    while(curservo->snum != snum) {
      curservo = curservo->nextservo;
      if (curservo == NULL) break;
    }

    if(curservo != NULL) {
      return number(curservo->servo.read());
    }
  }

  pfstring("Servo not found", (pfun_t)pserial);
  return nil;
}

/*
  (servo-detach)
  Detach servo snum, thus freeing the assigned pin for other tasks.
*/
object *fn_ServoDetach (object *args, object *env) {
  (void) env;

  int snum = checkinteger(first(args));
  curservo = servolist;
  struct ulservo* lastservo = servolist;

  if (curservo != NULL) {

    while(curservo->snum != snum) {
      lastservo = curservo;
      curservo = curservo->nextservo;
      if (curservo == NULL) break;
    }

    if(curservo != NULL) {
      curservo->servo.detach();
      if (curservo == servolist) {    // delete first element of list
        servolist = curservo->nextservo;
      }
      else {
        lastservo->nextservo = curservo->nextservo;
      }
      free(curservo);
      return tee;
    }
  }

  pfstring("Servo not found", (pfun_t)pserial);
  return nil;
}  


// Symbol names
const char stringLoadMono[] PROGMEM = "load-mono";

const char stringOledBegin[] PROGMEM = "oled-begin";
const char stringOledClear[] PROGMEM = "oled-clear";
const char stringOledSetRotation[] PROGMEM = "oled-set-rotation";
const char stringOledSetColor[] PROGMEM = "oled-set-color";
const char stringOledSetFont[] PROGMEM = "oled-set-font";
const char stringOledWriteChar[] PROGMEM = "oled-write-char";
const char stringOledWriteString[] PROGMEM = "oled-write-string";
const char stringOledDrawPixel[] PROGMEM = "oled-draw-pixel";
const char stringOledDrawLine[] PROGMEM = "oled-draw-line";
const char stringOledDrawHLine[] PROGMEM = "oled-draw-hline";
const char stringOledDrawVLine[] PROGMEM = "oled-draw-vline";
const char stringOledDrawRect[] PROGMEM = "oled-draw-rect";
const char stringOledFillRect[] PROGMEM = "oled-fill-rect";
const char stringOledDrawCircle[] PROGMEM = "oled-draw-circle";
const char stringOledFillCircle[] PROGMEM = "oled-fill-circle";
const char stringOledDrawRoundRect[] PROGMEM = "oled-draw-round-rect";
const char stringOledFillRoundRect[] PROGMEM = "oled-fill-round-rect";
const char stringOledFillTriangle[] PROGMEM = "oled-fill-triangle";
const char stringOledDisplayBMP[] PROGMEM = "oled-display-bmp";
const char stringOledShowBMP[] PROGMEM = "oled-show-bmp";

const char stringBMEBegin[] PROGMEM = "bme-begin";
const char stringBMEReadTemp[] PROGMEM = "bme-read-temp";
const char stringBMEReadHum[] PROGMEM = "bme-read-hum";

const char stringServoAttach[] PROGMEM = "servo-attach";
const char stringServoWrite[] PROGMEM = "servo-write";
const char stringServoWriteMicroseconds[] PROGMEM = "servo-write-microseconds";
const char stringServoRead[] PROGMEM = "servo-read";
const char stringServoDetach[] PROGMEM = "servo-detach";


// Documentation strings
const char docLoadMono[] PROGMEM = "(load-mono fname arr [offx] [offy])\n"
"Open monochrome BMP file fname from SD if it exits and copy it into the two-dimensional uLisp bit array provided.\n"
"Note that this allocates massive amounts of RAM. Use for small bitmaps/icons only.\n"
"When the image is larger than the array, only the upper leftmost area of the bitmap fitting into the array is loaded.\n"
"Providing offx and offy you may move the 'window' of the array to other parts of the bitmap (useful e.g. for tiling).";

const char docOledBegin[] PROGMEM = "(oled-begin adr)\n"
"Initialize OLED (optionally using I2C address adr, otherwise using default address #x3C (7 bit)/#x78 (8 bit)).";
const char docOledClear[] PROGMEM = "(oled-clear)\n"
"Clear OLED.";
const char docOledSetRotation[] PROGMEM = "(oled-set-rotation rot)\n"
"Set rotation of screen. 0 = no rotation, 1 = 90 degrees, 2 = 180 degrees, 3 = 270 degrees, 4 = hor. mirrored, 5 = vert. mirrored.";
const char docOledSetColor[] PROGMEM = "(oled-set-color fg)\n"
"Set foreground color for text and graphics. Colors are 0 (clear/black), 1 (set/white) and 2 (XOR).";
const char docOledSetFont[] PROGMEM = "(oled-set-font n)\n"
"Set text font number n. Values from 0 to 2 correspond to U8G2 font pixel\n"
"heights 8, 16 und 32 (font families scrum (default), inb16 and logisoso32).";
const char docOledWriteChar[] PROGMEM = "(oled-write-char x y c)\n"
"Write char c to screen at location x y.";
const char docOledWriteString[] PROGMEM = "(oled-write-string x y str)\n"
"Write string str to screen at location x y.";
const char docOledDrawPixel[] PROGMEM = "(oled-draw-pixel x y)\n"
"Draw pixel at position x y (using color set before)";
const char docOledDrawLine[] PROGMEM = "(oled-draw-line x0 y0 x1 y1)\n"
"Draw a line between positions x0/y0 and x1/y1.";
const char docOledDrawHLine[] PROGMEM = "(oled-draw-hline x y w)\n"
"Draw fast horizontal line at position X Y with length w.";
const char docOledDrawVLine[] PROGMEM = "(oled-draw-vline x y h)\n"
"Draw fast vertical line at position X Y with length h.";
const char docOledDrawRect[] PROGMEM = "(oled-draw-rect x y w h)\n"
"Draw empty rectangle at x y with width w and height h.";
const char docOledFillRect[] PROGMEM = "(oled-fill-rect x y w h)\n"
"Draw empty rectangle at x y with width w and height h.";
const char docOledDrawCircle[] PROGMEM = "(oled-draw-circle x y r)\n"
"Draw empty circle at position x y with radius r.";
const char docOledFillCircle[] PROGMEM = "(oled-fill-circle x y r)\n"
"Draw filled circle at position x y with radius r.";
const char docOledDrawRoundRect[] PROGMEM = "(oled-draw-round-rect x y w h r)\n"
"Draw empty rectangle at x y with width w and height h. Edges are rounded with radius r.";
const char docOledFillRoundRect[] PROGMEM = "(oled-fill-round-rect x y w h r)\n"
"Draw filled rectangle at x y with width w and height h. Edges are rounded with radius r.";
const char docOledFillTriangle[] PROGMEM = "(oled-fill-triangle x0 y0 x1 y1 x2 y2)\n"
"Draw filled triangle with corners at x0/y0, x1/y1 and x2/y2.";
const char docOledDisplayBMP[] PROGMEM = "(oled-display-bmp fname x y)\n"
"Open monochrome BMP file fname from SD if it exits and display it on screen at position x y (using the color set before).";
const char docOledShowBMP[] PROGMEM = "(oled-show-bmp arr x y)\n"
"Show bitmap image contained in monochrome uLisp array arr on screen at position x y (using color set before).";

const char docBMEBegin[] PROGMEM = "(bme-begin [adr])\n"
"Initialize sensor (optionally using I2C address adr, otherwise using default address #x77).";
const char docBMEReadTemp[] PROGMEM = "(bme-read-temp)\n"
"Read temperature (degree celsius).";
const char docBMEReadHum[] PROGMEM = "(bme-read-hum)\n"
"Read humidity (percent).";

const char docServoAttach[] PROGMEM = "(servo-attach snum pin usmin usmax)\n"
"Attach servo snum to pin. Optionally define new pulse width min/max in microseconds.";
const char docServoWrite[] PROGMEM = "(servo-write snum angle)\n"
"Set angle of servo snum in degrees (0 to 180).";
const char docServoWriteMicroseconds[] PROGMEM = "(servo-write snum usecs)\n"
"Set angle of servo snum using a pulse width value in microseconds.";
const char docServoRead[] PROGMEM = "(servo-read snum)\n"
"Read current angle of servo snum in degrees.";
const char docServoDetach[] PROGMEM = "(servo-detach snum)\n"
"Detach servo snum, thus freeing the assigned pin for other tasks.";


// Symbol lookup table
const tbl_entry_t lookup_table2[] PROGMEM = {

{ stringLoadMono, fn_LoadMono, 0224, docLoadMono },

{ stringOledBegin, fn_OledBegin, 0201, docOledBegin },
{ stringOledClear, fn_OledClear, 0200, docOledClear },
{ stringOledSetRotation, fn_OledSetRotation, 0211, docOledSetRotation },
{ stringOledSetColor, fn_OledSetColor, 0211, docOledSetColor },
{ stringOledSetFont, fn_OledSetFont, 0211, docOledSetFont },
{ stringOledWriteChar, fn_OledWriteChar, 0233, docOledWriteChar },
{ stringOledWriteString, fn_OledWriteString, 0233, docOledWriteString },
{ stringOledDrawPixel, fn_OledDrawPixel, 0222, docOledDrawPixel },
{ stringOledDrawLine, fn_OledDrawLine, 0244, docOledDrawLine },
{ stringOledDrawHLine, fn_OledDrawHLine, 0233, docOledDrawHLine },
{ stringOledDrawVLine, fn_OledDrawVLine, 0233, docOledDrawVLine },
{ stringOledDrawRect, fn_OledDrawRect, 0244, docOledDrawRect },
{ stringOledFillRect, fn_OledFillRect, 0244, docOledFillRect },
{ stringOledDrawCircle, fn_OledDrawCircle, 0233, docOledDrawCircle },
{ stringOledFillCircle, fn_OledFillCircle, 0233, docOledFillCircle },
{ stringOledDrawRoundRect, fn_OledDrawRoundRect, 0255, docOledDrawRoundRect },
{ stringOledFillRoundRect, fn_OledFillRoundRect, 0255, docOledFillRoundRect },
{ stringOledFillTriangle, fn_OledFillTriangle, 0266, docOledFillTriangle },
{ stringOledDisplayBMP, fn_OledDisplayBMP, 0233, docOledDisplayBMP },
{ stringOledShowBMP, fn_OledShowBMP, 0233, docOledShowBMP },

{ stringBMEBegin, fn_BMEBegin, 0201, docBMEBegin },
{ stringBMEReadTemp, fn_BMEReadTemp, 0200, docBMEReadTemp },
{ stringBMEReadHum, fn_BMEReadHum, 0200, docBMEReadHum },

{ stringServoAttach, fn_ServoAttach, 0224, docServoAttach },
{ stringServoWrite, fn_ServoWrite, 0222, docServoWrite },
{ stringServoWriteMicroseconds, fn_ServoWriteMicroseconds, 0222, docServoWriteMicroseconds },
{ stringServoRead, fn_ServoRead, 0211, docServoRead },
{ stringServoDetach, fn_ServoDetach, 0211, docServoDetach },

};

// Table cross-reference functions - do not edit below this line

tbl_entry_t *tables[] = {lookup_table, lookup_table2};
const unsigned int tablesizes[] = { arraysize(lookup_table), arraysize(lookup_table2) };

const tbl_entry_t *table (int n) {
  return tables[n];
}

unsigned int tablesize (int n) {
  return tablesizes[n];
}
