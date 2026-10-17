#include "hosyond_battery.h"
#ifdef HOSYOND_CYD40
namespace HosyondBattery {
static Reading battery = {0,0,false,false};
static uint32_t lastSampleMs=0;
static uint16_t history[30]={};
static uint8_t historyCount=0, historyAt=0;
static const uint16_t mvCurve[]={3300,3500,3600,3700,3750,3800,3850,3900,4000,4100,4200};
static const uint8_t pctCurve[]={0,5,10,20,30,40,50,60,75,90,100};

static uint8_t percent(uint16_t mv) {
    if (mv <= mvCurve[0]) return 0;
    for (uint8_t i=1;i<sizeof(mvCurve)/sizeof(mvCurve[0]);i++) {
        if(mv<=mvCurve[i])
            return (uint8_t)(pctCurve[i-1]+
                (uint32_t)(pctCurve[i]-pctCurve[i-1])*(mv-mvCurve[i-1])/
                (mvCurve[i]-mvCurve[i-1]));
    }
    return 100;
}

const Reading& sample() {
    uint32_t now=millis();
    if (lastSampleMs && now-lastSampleMs<2000) return battery;
    lastSampleMs=now?now:1;
    uint32_t sum=0;
    for(uint8_t i=0;i<8;i++)sum+=analogReadMilliVolts(34);
    battery.mv=(uint16_t)((sum/8)*2); // Hosyond CYD40 2:1 LiPo divider
    battery.present=battery.mv>=2500;
    if(!battery.present){
        battery.pct=0;
        battery.chargingEstimated=false;
        historyCount=historyAt=0;
        return battery;
    }
    battery.pct=percent(battery.mv);
    history[historyAt]=battery.mv;
    historyAt=(uint8_t)((historyAt+1)%30);
    if(historyCount<30)historyCount++;
    uint16_t lo=UINT16_MAX,hi=0;
    for(uint8_t i=0;i<historyCount;i++){
        if(history[i]<lo)lo=history[i];
        if(history[i]>hi)hi=history[i];
    }
    // Matches SquachWatch CYD40's measured-voltage heuristic.
    // Rising at least 40mV from recent low suggests charging, and a
    // 40mV drop from recent peak suggests disconnection.
    // This is NOT charger-hardware detection.
    if(!battery.chargingEstimated && battery.mv>=lo+40)
        battery.chargingEstimated=true;
    else if(battery.chargingEstimated && battery.mv+40<=hi)
        battery.chargingEstimated=false;
    return battery;
}
void drawChargingBolt(TFT_eSPI& tft,int16_t x,int16_t y,uint16_t color) {
    tft.fillTriangle(x+5,y,x+1,y+7,x+5,y+7,color);
    tft.fillTriangle(x+3,y+6,x+8,y+6,x+2,y+13,color);
}
}
#endif
