
#ifndef __EPC_DEF_H
#define __EPC_DEF_H

#include "stm32f1xx.h"

//регистры данных STPM01
#pragma pack( push, 1 )

typedef struct {
    //DAP
    unsigned status : 8;            //статус STPM01
    unsigned act_energy : 20;       //счетчик активной энергии
    unsigned dap_parity : 4;        //четность блока данных
    //DRP
    unsigned upper_fu : 6;
    unsigned const1_drp : 1;        //константа
    unsigned const2_drp : 1;        //константа
    unsigned rea_energy : 20;       //счетчик реактивной энергии
    unsigned drp_parity : 4;        //четность блока данных
    //DSP
    unsigned lower_fu : 8;
    unsigned app_energy : 20;       //счетчик общей энергии
    unsigned dsp_parity : 4;        //четность блока данных
    //DFP
    unsigned mode_signal : 8;
    unsigned type1_energ : 20;
    unsigned dfp_parity : 4;        //четность блока данных
    //DEV среднеквадратичные значения измеренного напряжения и тока
    unsigned i_rms : 16;
    unsigned u_rms : 11;
    unsigned dev_p : 1;             //константа
    unsigned dev_parity : 4;        //четность блока данных
    //DMV мгновенные значения измеренного напряжения и тока
    unsigned i_mom : 16;
    unsigned u_mom : 11;
    unsigned dmv_p : 1;             //константа
    unsigned dmv_parity : 4;        //четность блока данных
    /*//CFL короткий формат
    unsigned lower_conf : 28;
    unsigned cfl_parity : 4;
    //CFH
    unsigned upper_conf : 28;
    unsigned cfh_parity : 4;*/
    //CFL полный формат
    unsigned config_tstd : 1;
    unsigned config_mdiv : 1;
    unsigned config_rc : 1;
    unsigned config_apl : 2;
    unsigned config_pst : 3;
    unsigned config_frs : 1;
    unsigned config_msbf : 1;
    unsigned config_fund : 1;
    unsigned config_reserv1 : 1;
    unsigned config_ltch : 2;
    unsigned config_kmot : 2;
    unsigned config_reserv2 : 2;
    unsigned config_bgtc : 2;
    unsigned config_cph : 4;
    unsigned config_chv1 : 4;
    unsigned cfl_parity : 4;        //четность блока данных
    //CFH
    unsigned config_chv2 : 4;
    unsigned config_chp : 8;
    unsigned config_chs : 8;
    unsigned config_crc : 2;
    unsigned config_nom : 2;
    unsigned config_addg : 1;
    unsigned config_crit : 1;
    unsigned config_lvs : 1;
    unsigned config_reserv3 : 1;
    unsigned cfh_parity : 4;        //четность блока данных
    //дополнительные данные, не входят в данные STPM01
    unsigned config_chv : 8;        //результируещее значение для config_chv1 и config_chv2
    unsigned data_parity : 1;       //проверка четности данных: 0/1 - ошибка/ОК
    unsigned data_valid : 1;        //достоверность данных: 0/1 - ошибка/ОК
 } EPC_struct;

#pragma pack( pop )

//**********************************************************************************
// Константы расчета Urms
//**********************************************************************************
#define STPM_R1         750000
#define STPM_R2         470
#define STPM_VREF       1.23
#define STPM_AU         4
#define STPM_KU         0.875
#define STPM_KINT_COMP  1.004
#define STPM_KINT       0.815
#define STPM_KDIF       0.6135
#define STPM_LENU       2048
#define STPM_KUT        2

//**********************************************************************************
// Константы расчета Irms
//**********************************************************************************
#define STPM_KS         0.00038
#define STPM_AI         32
#define STPM_KI         0.875
#define STPM_LENI       65536

//**********************************************************************************
// Состояние статуса STPM01
//**********************************************************************************
#define STATUS_BIL      0x01    //No load condition
                                //BIL=0: No load condition not detected
                                //BIL=1: No load detected
#define STATUS_BCF      0x02    //signals status
                                //BCF=0: ? ? signals alive
                                //BCF=1: one or both ? ? signals are stacked
#define STATUS_BFR      0x04    //Line frequency range
                                //BFR=0: Line frequency inside the 45Hz-65Hz range
                                //BFR=1: Line frequency out of range
#define STATUS_BIT      0x08    //Tamper condition
                                //BIT=0: Tamper not detected;
                                //BIT=1: Tamper detected;
#define STATUS_MUX      0x10    //Current channel selection
                                //MUX=0: Primary current channels selected by the tamper module;
                                //MUX=1: Secondary current channels selected by the tamper module;
#define STATUS_LIN      0x20    //Trend of the line voltage
                                //LIN=0: line voltage is going from the minimum to the maximum value. (?v/?t > 0);
                                //LIN=1: line voltage is going from the maximum to the minimum value. (?v/?t < 0);
#define STATUS_PIN      0x40    //Output pins check
                                //PIN=0: the output pins are consistent with the data
                                //PIN=1: the output pins are different with the data, this means some output pin is forced to 1 or 0.
#define STATUS_HLT      0x80    //Data Validity
                                //HLT=0: the data records reading are valid.
                                //HLT=1: the data records are not valid. A reset occurred and a restart is in progress.

//**********************************************************************************
// Режимы STPM01
//**********************************************************************************
#define MODE_BANK       0x01    //Used for RC startup procedure
#define MODE_PUMP       0x02    //0 = MOP and MON operates
                                //1 = MOP and MON provides the driving signals to implement a 
                                //charge-pump DC-DC converter
#define MODE_CSEL       0x10    //0 = Current Channel 1 selected when tamper is disabled
                                //1 = Channel 2 selected when tamper is disabled
#define MODE_RD         0x20    //0 = The 56 Configuration bits originated by OTP antifuses
                                //1 = The 56 Configuration bits originated by shadow latches 
#define MODE_WE         0x40    //0 = Any writing in the configuration bits is recorded in the shadow latches
                                //1 = Any writing in the configuration bits is recorded both in the
                                //shadow latches and in the OTP antifuse elements
#define MODE_PRECHARGE  0x80    //Swap the 32 bits data records reading.
                                //From 1,2,3,4,5,6,7,8, to 5,6,7,8,1,2,3,4 and viceversa

//**********************************************************************************
// Регистры режима STPM01
//**********************************************************************************
#define CONFIG_MODE_RD      61
#define CONFIG_MODE_RD_SZ   1
#define CONFIG_MODE_RD_BIT  61  //0 = The 56 Configuration bits originated by OTP antifuses
                                //1 = The 56 Configuration bits originated by shadow latches

//**********************************************************************************
// Регистры конфигурации STPM01
// Имя регистра, размер в битах, номер бита
//**********************************************************************************
#define CONFIG_TSTD         0
#define CONFIG_TSTD_SZ      1
#define CONFIG_TSTD_BIT     0   //CFL=0x00000001 Test mode and OTP write disable
                                //               TSTD=0: testing and continuous pre-charge of OTP when in read mode
                                //               TSTD=1:normal operation and no more writes to OTP
#define CONFIG_MDIV         1
#define CONFIG_MDIV_SZ      1
#define CONFIG_MDIV_BIT     1   //CFL=0x00000002 Measurement frequency range selection
                                //               MDIV=0: 4.000MHz to 4.194MHz
                                //               MDIV=1: 8.000MHz to 8.192MHz
#define CONFIG_RC           2
#define CONFIG_RC_SZ        1
#define CONFIG_RC_BIT       2   //CFL=0x00000004 Type of internal oscillator selection
                                //               RC=0:crystal oscillator
                                //               RC=1:RC oscillator
#define CONFIG_APL          3         
#define CONFIG_APL_SZ       2  
#define CONFIG_APL0_BIT     3   //CFL=0x00000008 Peripheral or Standalone mode
#define CONFIG_APL1_BIT     4   //CFL=0x00000010 APL=0: peripheral, MON=WatchDOG; MOP=ZCR, LED=pulses
                                //               APL=1: peripheral, MOP=?? Voltage; MON=?? current; LED=Mux (current)
                                //               APL=2: standalone, MOP,MON=stepper, LED=pulses, SCLNLC=no load
                                //               APL=3: standalone, MOP:MON=stepper, LED=pulses according to KMOT
#define CONFIG_PST          5
#define CONFIG_PST_SZ       3
#define CONFIG_PST0_BIT     5   //CFL=0x00000020 Current channel sensor type, gain and tamper selection
#define CONFIG_PST1_BIT     6   //CFL=0x00000040 PST=0: primary is coil x8 (x16 if ADDG=1), secondary is not used, no tamper
#define CONFIG_PST2_BIT     7   //CFL=0x00000080 PST=1: primary is coil x24 (x32 if ADDG=1), secondary is not used, no tamper
                                //               PST=2: primary is CT x8, secondary is not used, no tamper
                                //               PST=3: primary is shunt x32, secondary is not used, no tamper
                                //               PST=4: primary is coil x8 (x16 if ADDG=1), secondary is coil x8 (x16 if ADDG=1), tamper
                                //               PST=5: primary is coil x24 (x32 if ADDG=1), secondary is coil x24 (x32 if ADDG=1), tamper
                                //               PST=6: primary is CT x8, secondary is CT x8, tamper
                                //               PST=7: primary is CT x8, secondary is shunt x32, tamper
#define CONFIG_FRS          8
#define CONFIG_FRS_SZ       1
#define CONFIG_FRS_BIT      8   //CFL=0x00000100 Power calculation when BFR=1 and PST?4,5 (no single wire mode)
                                //               FRS=0: energy accumulation is frozen, power is set to zero
                                //               FRS=1: normal energy accumulation and power computation (p=u*i)
#define CONFIG_MSBF         9
#define CONFIG_MSBF_SZ      1
#define CONFIG_MSBF_BIT     9   //CFL=0x00000200 Bit sequence output during record data reading selection
                                //               MSBF=0: msb first
                                //               MSBF=1: lsb first
#define CONFIG_FUND         10
#define CONFIG_FUND_SZ      1
#define CONFIG_FUND_BIT     10  //CFL=0x00000400 This bit swap the information stored in the type0 (first 20 bits of DAP register)
                                //               and type1 (first 20 bits of DFP register) active energy
                                //               FUND=0: type 0 contains wide band active energy, type1 contains fundamental active energy
                                //               FUND=1: type 0 contains fundamental active energy, type1 contains wide band active energy
#define CONFIG_LTCH         12
#define CONFIG_LTCH_SZ      2
#define CONFIG_LTCH0_BIT    12  //CFL=0x00001000 No load condition threshold as product between VRMS and IRMS
#define CONFIG_LTCH1_BIT    13  //CFL=0x00002000 LTCH=0 - 800, LTCH=1 - 1600, LTCH=2 - 3200, LTCH=3 - 6400 

#define CONFIG_KMOT         14
#define CONFIG_KMOT_SZ      2
#define CONFIG_KMOT0_BIT    14  //CFL=0x00004000 Constant of stepper pulses/kWh selection when APL=2 or 3
#define CONFIG_KMOT1_BIT    15  //CFL=0x00008000 If LVS=0, KMOT=0 P/64 KMOT=1 P/128 KMOT=2 P/32 KMOT=3 P/256
                                //               If LVS=1, KMOT=0 P/640 KMOT=1 P/1280 KMOT=2 P/320 KMOT=3 P/2560
                                //               Selection of pulses for LED when APL=0:
                                //               KMOT=0 Type0 Active Energy
                                //               KMOT=1 Type1 Active Energy
                                //               KMOT=2 Reactive Energy
                                //               KMOT=3 Apparent Energy
#define CONFIG_BGTC         18
#define CONFIG_BGTC_SZ      2
#define CONFIG_BGTC0_BIT    18  //CFL=0x00040000 Bandgap Temperature compensation bits
#define CONFIG_BGTC1_BIT    19  //CFL=0x00080000 

#define CONFIG_CPH          20
#define CONFIG_CPH_SZ       4
#define CONFIG_CPH0_BIT     20  //CFL=0x00100000 4-bit unsigned data for compensation of phase error, 0°+0.576°
#define CONFIG_CPH1_BIT     21  //CFL=0x00200000 16 values are possible with a compensation step of 0.0384°. When CPH=0 the
#define CONFIG_CPH2_BIT     22  //CFL=0x00400000 compensation is 0°, when CPH=15 the compensation is 0.576°.
#define CONFIG_CPH3_BIT     23  //CFL=0x00800000 

#define CONFIG_CHV          24
#define CONFIG_CHV_SZ       8
#define CONFIG_CHV0_BIT     24  //CFL=0x01000000 8-bit unsigned data for voltage channel calibration
#define CONFIG_CHV1_BIT     25  //CFL=0x02000000 256 values are possible. When CHV is 0 the calibrator is at -12.5 % of the
#define CONFIG_CHV2_BIT     26  //CFL=0x04000000 nominal value. When CHV is 255 the calibrator is at +12.5 %. The calibration
#define CONFIG_CHV3_BIT     27  //CFL=0x08000000 step is then 0.098%
#define CONFIG_CHV4_BIT     28  //CFH=0x00000001
#define CONFIG_CHV5_BIT     29  //CFH=0x00000002 
#define CONFIG_CHV6_BIT     30  //CFH=0x00000004
#define CONFIG_CHV7_BIT     31  //CFH=0x00000008

#define CONFIG_CHP          32
#define CONFIG_CHP_SZ       8
#define CONFIG_CHP0_BIT     32  //CFH=0x00000010 8-bit unsigned data for primary current channel calibration.
#define CONFIG_CHP1_BIT     33  //CFH=0x00000020 256 values are possible. When CHP is 0 the calibrator is at -12.5 % of the
#define CONFIG_CHP2_BIT     34  //CFH=0x00000040 nominal value. When CHP is 255 the calibrator is at +12.5 %. The calibration
#define CONFIG_CHP3_BIT     35  //CFH=0x00000080 step is then 0.098%.
#define CONFIG_CHP4_BIT     36  //CFH=0x00000100 
#define CONFIG_CHP5_BIT     37  //CFH=0x00000200 
#define CONFIG_CHP6_BIT     38  //CFH=0x00000400 
#define CONFIG_CHP7_BIT     39  //CFH=0x00000800 

#define CONFIG_CHS          40
#define CONFIG_CHS_SZ       8
#define CONFIG_CHS0_BIT     40  //CFH=0x00001000 8-bit unsigned data for secondary current channel calibration.
#define CONFIG_CHS1_BIT     41  //CFH=0x00002000 256 values are possible. When CHS is 0 the calibrator is at -12.5 % of the
#define CONFIG_CHS2_BIT     42  //CFH=0x00004000 nominal value. When CHS is 255 the calibrator is at +12.5 %. The calibration
#define CONFIG_CHS3_BIT     43  //CFH=0x00008000 step is then 0.098 %.
#define CONFIG_CHS4_BIT     44  //CFH=0x00010000 
#define CONFIG_CHS5_BIT     45  //CFH=0x00020000 
#define CONFIG_CHS6_BIT     46  //CFH=0x00040000 
#define CONFIG_CHS7_BIT     47  //CFH=0x00080000 

#define CONFIG_CRC          48
#define CONFIG_CRC_SZ       2
#define CONFIG_CRC0_BIT     48  //CFH=0x00100000 2-bit unsigned data for calibration of RC oscillator
#define CONFIG_CRC1_BIT     49  //CFH=0x00200000 CRC=0, or CRC=3 cal=0% CRC=1, cal=+10%; CRC=2, cal=-10%.

#define CONFIG_NOM          50
#define CONFIG_NOM_SZ       2
#define CONFIG_NOM0_BIT     50  //CFH=0x00400000 2-bit modifier of nominal voltage for Single Wire Meter
#define CONFIG_NOM1_BIT     51  //CFH=0x00800000 NOM=0: KNOM=0.3594 / NOM=1: KNOM=0.3906 / NOM=2: KNOM=0.4219 / NOM=3: KNOM=0.4531

#define CONFIG_ADDG         52
#define CONFIG_ADDG_SZ      1
#define CONFIG_ADDG_BIT     52  //CFH=0x01000000 Selection of additional gain on current channels: ADDG=0: Gain+=0 / ADDG=1: Gain+=8

#define CONFIG_CRIT         53
#define CONFIG_CRIT_SZ      1
#define CONFIG_CRIT_BIT     53  //CFH=0x02000000 Selection of tamper threshold: CRIT=0: 12,5% / CRIT=1: 6,25%

#define CONFIG_LVS          54
#define CONFIG_LVS_SZ       1
#define CONFIG_LVS_BIT      54  //CFH=0x04000000 Type of stepper selection: LVS=0: pulse width 31.25 ms, 5V, / LVS=1: pulse width, 156.25 ms, 3V 

#endif
