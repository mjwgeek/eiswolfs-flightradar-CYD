#include "touch_input.h"
#include "config.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#ifdef HOSYOND_CYD40
#include <TFT_eSPI.h>
// TFT_eSPI owns the shared ST7796S / XPT2046 SPI bus.
extern TFT_eSPI tft;
#endif

namespace TouchInput {

namespace {
#ifndef HOSYOND_CYD40
    // The original 2.8-inch board has a dedicated touch SPI peripheral.
    SPIClass touchSpi(VSPI);
    XPT2046_Touchscreen touch(Config::TOUCH_CS_PIN, Config::TOUCH_IRQ_PIN);
#endif

    constexpr int16_t MARGIN_PX = 24; // Abstand der Kalibrierungspunkte vom Rand

    // Rohe Min/Max-Werte aus der Kalibrierung. Bis zur ersten Kalibrierung
    // grosszuegige Standardwerte, damit die Kalibrierungs-Routine selbst schon
    // grob nutzbare (wenn auch ungenaue) Koordinaten bekommt.
    int16_t calXmin = 200;
    int16_t calXmax = 3900;
    int16_t calYmin = 200;
    int16_t calYmax = 3900;
    bool calibrationLoaded = false;
#ifdef HOSYOND_CYD40
    // A 2x2 affine coordinate basis (4 real corner samples) handles
    // swapped raw axes, negative slopes and digitizers mounted rotated.
    // Values live in the exact 320x480 coordinates drawn by calibration.
    struct Affine {
        float midX=0, midY=0;
        float rightX=0, rightY=0;
        float downX=0, downY=0;
        float determinant=0;
        bool baselineRotated180=false;
        bool valid=false;
        Point corners[4]; // persisted so the fit can be restored losslessly
    };
    Affine affine;
    bool fitCorners(const Point corner[4], bool baselineRotated180) {
        // Index: top-left, top-right, bottom-right, bottom-left.
        int32_t mx=0,my=0;
        for(int i=0;i<4;i++){mx+=corner[i].x;my+=corner[i].y;}
        const float centerX=mx*0.25f, centerY=my*0.25f;
        const float rightX=((corner[1].x-corner[0].x)+(corner[2].x-corner[3].x))*0.5f;
        const float rightY=((corner[1].y-corner[0].y)+(corner[2].y-corner[3].y))*0.5f;
        const float downX=((corner[3].x-corner[0].x)+(corner[2].x-corner[1].x))*0.5f;
        const float downY=((corner[3].y-corner[0].y)+(corner[2].y-corner[1].y))*0.5f;
        const float determinant=rightX*downY-rightY*downX;
        // Both directions should traverse a meaningful portion of the
        // XPT2046 12-bit range and should not be almost parallel.
        const float normR=rightX*rightX+rightY*rightY;
        const float normD=downX*downX+downY*downY;
        if(normR<40000 || normD<40000 ||
           determinant*determinant<0.01f*normR*normD) return false;
        // Reject accidentally doubled taps / wildly mismatched corners.
        const Point& tl=corner[0], &tr=corner[1], &br=corner[2], &bl=corner[3];
        const float closureX=tl.x+br.x-tr.x-bl.x;
        const float closureY=tl.y+br.y-tr.y-bl.y;
        if(closureX*closureX+closureY*closureY > 0.10f*(normR+normD)) return false;
        affine.midX=centerX; affine.midY=centerY;
        affine.rightX=rightX; affine.rightY=rightY;
        affine.downX=downX; affine.downY=downY;
        affine.determinant=determinant;
        affine.baselineRotated180=baselineRotated180;
        for(int i=0;i<4;i++) affine.corners[i]=corner[i];
        affine.valid=true;
        calibrationLoaded=true;
        return true;
    }
#endif

    bool lastTouched = false;
    Point lastMapped;  // release events must report calibrated, not raw ADC coordinates

    // TEST DIAG - siehe touch_input.h::lastRawSample(). Rein additiv,
    // wird nur in rawPoint() unten passiv mitgeschrieben, beeinflusst
    // keinerlei bestehende Kalibrierungs-/Tap-Logik.
    RawSample lastRawSampleValue;

    // Siehe setRotated180() in touch_input.h - Default false (normale
    // Ausrichtung), wird beim Boot bzw. beim Umschalten im Menue gesetzt.
    bool rotated180 = false;

    // Zeitpunkt des letzten abgeschlossenen Taps (siehe wasTapped()/
    // msSinceLastTap()) - fuer den Menue-Inaktivitaets-Timeout.
    uint32_t lastTapMs = 0;

    int16_t clampi(int16_t v, int16_t lo, int16_t hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

void begin() {
#ifdef HOSYOND_CYD40
    // Shared panel SPI: never call SPIClass.begin() a second time on these
    // pins or the XPT2046 and ST7796S will fight over the same host.
    // The TFT_eSPI touch implementation uses TOUCH_CS=33 from the setup.
    pinMode(Config::TOUCH_CS_PIN, OUTPUT);
    digitalWrite(Config::TOUCH_CS_PIN, HIGH);
    Serial.println("[CYD40] Touch on TFT_eSPI shared SPI bus");
#else
    touchSpi.begin(Config::TOUCH_CLK_PIN, Config::TOUCH_MISO_PIN,
                    Config::TOUCH_MOSI_PIN, Config::TOUCH_CS_PIN);
    touch.begin(touchSpi);
    touch.setRotation(0);
#endif
    lastTapMs = millis();
}

void setRotated180(bool rotated) {
    rotated180 = rotated;
}

bool hasCalibration() { return calibrationLoaded; }

void setCalibration(int16_t rawXmin, int16_t rawXmax, int16_t rawYmin, int16_t rawYmax) {
    calXmin = rawXmin;
    calXmax = rawXmax;
    calYmin = rawYmin;
    calYmax = rawYmax;
    calibrationLoaded = true;
}

#ifdef HOSYOND_CYD40
bool setCalibrationFromCorners(const Point corners[4]) {
    return fitCorners(corners, rotated180);
}
#endif

bool loadCalibration() {
    if (!SD.exists(Config::SD_CALIBRATION_FILE)) return false;

    File f = SD.open(Config::SD_CALIBRATION_FILE, FILE_READ);
    if (!f) return false;

    String line = f.readStringUntil('\n');
    f.close();
    line.trim();

#ifdef HOSYOND_CYD40
    // Prior preview stored sorted min/max in the *wrong axis order*.
    // Never silently reuse it: prompt for one proper four-corner calibration.
    int rotation=0, x0,y0,x1,y1,x2,y2,x3,y3;
    if(sscanf(line.c_str(),"H40V2,%d,%d,%d,%d,%d,%d,%d,%d,%d",
              &rotation,&x0,&y0,&x1,&y1,&x2,&y2,&x3,&y3)!=9)
        return false;
    if(rotation<0||rotation>1)return false;
    Point corners[4];
    const int xx[]={x0,x1,x2,x3}, yy[]={y0,y1,y2,y3};
    for(int i=0;i<4;i++) {
        if(xx[i]<0||xx[i]>4095||yy[i]<0||yy[i]>4095)return false;
        corners[i].x=xx[i]; corners[i].y=yy[i];
    }
    return fitCorners(corners,rotation!=0);
#else
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) return false;

    int16_t xmin = line.substring(0, p1).toInt();
    int16_t xmax = line.substring(p1 + 1, p2).toInt();
    int16_t ymin = line.substring(p2 + 1, p3).toInt();
    int16_t ymax = line.substring(p3 + 1).toInt();

    if (xmax <= xmin || ymax <= ymin) return false;

    setCalibration(xmin, xmax, ymin, ymax);
    return true;
#endif
}

void saveCalibration() {
    File f = SD.open(Config::SD_CALIBRATION_FILE, FILE_WRITE);
    if (!f) return;
#ifdef HOSYOND_CYD40
    if(!affine.valid) { f.close(); return; }
    f.printf("H40V2,%d",affine.baselineRotated180?1:0);
    for(int i=0;i<4;i++)
        f.printf(",%d,%d",affine.corners[i].x,affine.corners[i].y);
    f.print("\n");
#else
    f.printf("%d,%d,%d,%d\n", calXmin, calXmax, calYmin, calYmax);
#endif
    f.close();
}

Point rawPoint() {
    Point p;
#ifdef HOSYOND_CYD40
    // SquachWatch CYD40 pattern: pressure-gate FIRST because
    // TFT_eSPI::getTouchRaw() itself reports values even with no finger.
    const uint16_t pressure = tft.getTouchRawZ();
    p.touched = pressure >= 350;
    if (p.touched) {
        uint16_t rx=0, ry=0;
        tft.getTouchRaw(&rx,&ry);
        p.x = rx; p.y = ry;
        lastRawSampleValue.x = rx;
        lastRawSampleValue.y = ry;
        lastRawSampleValue.z = pressure;
    }
#else
    p.touched = touch.touched();
    if (p.touched) {
        TS_Point raw = touch.getPoint();
        p.x = raw.x;
        p.y = raw.y;
        lastRawSampleValue.x = raw.x;
        lastRawSampleValue.y = raw.y;
        lastRawSampleValue.z = raw.z;
    }
#endif
    return p;
}

// TEST DIAG - siehe touch_input.h.
RawSample lastRawSample() {
    return lastRawSampleValue;
}

Point mappedPoint() {
    Point raw = rawPoint();
    Point out;
    out.touched = raw.touched;
    if (!raw.touched) return out;

#ifdef HOSYOND_CYD40
    if(affine.valid) {
        const float dx=raw.x-affine.midX,dy=raw.y-affine.midY;
        // Invert the raw basis. This correctly handles swapped and
        // reversed axes, unlike separate sorted raw min/max values.
        const float u=(dx*affine.downY-dy*affine.downX)/affine.determinant;
        const float v=(affine.rightX*dy-affine.rightY*dx)/affine.determinant;
        float px=Config::SCREEN_WIDTH*0.5f+
                 u*(Config::SCREEN_WIDTH-2*MARGIN_PX);
        float py=Config::SCREEN_HEIGHT*0.5f+
                 v*(Config::SCREEN_HEIGHT-2*MARGIN_PX);
        // The calibration targets were displayed in whatever orientation
        // was active at calibration time. Reflect only if later changed.
        if(rotated180!=affine.baselineRotated180) {
            px=(Config::SCREEN_WIDTH-1)-px;
            py=(Config::SCREEN_HEIGHT-1)-py;
        }
        out.x=clampi((int16_t)lroundf(px),0,Config::SCREEN_WIDTH-1);
        out.y=clampi((int16_t)lroundf(py),0,Config::SCREEN_HEIGHT-1);
        return out;
    }
#endif
    long mx = map((long)raw.x, calXmin, calXmax, MARGIN_PX, Config::SCREEN_WIDTH - MARGIN_PX);
    long my = map((long)raw.y, calYmin, calYmax, MARGIN_PX, Config::SCREEN_HEIGHT - MARGIN_PX);

    if (rotated180) {
        // Touch-Chip-Koordinaten sind an die physische Panel-Ausrichtung
        // gebunden, nicht an tft.setRotation() - bei gedrehtem Display muss
        // hier zusaetzlich gespiegelt werden, sonst zeigen Taps auf den
        // (visuell) verschobenen Buttons ins Leere.
        mx = (Config::SCREEN_WIDTH - 1) - mx;
        my = (Config::SCREEN_HEIGHT - 1) - my;
    }

    out.x = clampi((int16_t)mx, 0, Config::SCREEN_WIDTH - 1);
    out.y = clampi((int16_t)my, 0, Config::SCREEN_HEIGHT - 1);
    return out;
}

bool wasTapped(Point& outPoint) {
    Point cur = mappedPoint();

    bool fired = false;
    if (!cur.touched && lastTouched) {
        // Critical regression fix: the previous version returned the raw
        // sample even though mappedPoint() had calibrated the press.
        outPoint = lastMapped;
        fired = true;
    }

    lastTouched = cur.touched;
    if (cur.touched) lastMapped = cur;

    if (fired) lastTapMs = millis();

    return fired;
}

uint32_t msSinceLastTap() {
    return millis() - lastTapMs;
}

}