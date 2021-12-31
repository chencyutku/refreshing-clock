#ifndef TEMP_LED_PW_ALARM
#define TEMP_LED_PW_ALARM

#define MAX_BRIGHTNESS_LEVEL 1200

// 時鐘、鬧鐘參數
#define SECS_A_DAY 86400        // 86400s a day, but reduce it for demostrate
#define NO_ALARM 160000         // 遠超一天的時間就不會觸發鬧鐘與色溫
#define TEMP_PREACT_TIME 3600   // 前一小時開始改變色溫
#define MORNING 25200           // 早上七點設為邊界
#define EVENING 68400           // 晚上七點設為邊界
#define DAY     1
#define NIGHT   0
#define PRESET_ABS_SEC 0
#define PRESET_ALARM_SEC 160000

// 時鐘模式
#define CUR_TIME  0
#define SET_TIME  1
#define SET_ALARM 2
#define MATH_EXPRESSION 3

#define CLEAR_TIME_SET  setting_sec = 0;\
                        time_set_cursor = 0;\
                        time_set_hr_ten = 0;

// 給鬧鐘鈴聲
#define BEE_NBR           4                     /*!< Number of bee after reset                              */
#define BEE_FREQ          3000                  /*!< Frequency of bee                                       */
#define BEE_ACTIVE_TIME   (BEE_FREQ * 5) / 100  /*!< Active 50mS per bee cycle                              */
#define BEE_INACTIVE_TIME (BEE_FREQ * 5) / 100  /*!< Inactive 50mS per bee cycle                            */
#define DO 0
#define RE 1
#define MI 2
#define FA 3
#define SO 4
#define LA 5
#define SI 6
#define DO_H 7
#define FREQ_DO 261.6
#define FREQ_RE 293.7
#define FREQ_MI 329.6
#define FREQ_FA 349.2
#define FREQ_SO 392.0
#define FREQ_LA 440
#define FREQ_SI 493.9
#define FREQ_DO_H 523.3
#define DELAY_LONG   5000000
#define DELAY_MIDDLE 4000000
#define DELAY_SHORT  1000000

#endif
