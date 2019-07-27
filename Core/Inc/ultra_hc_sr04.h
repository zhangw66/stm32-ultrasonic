#ifndef __ULTRA_HC_SR04_H__
#define __ULTRA_HC_SR04_H__
#include <sensor_ultrasonic.h>
#define MEISSA_HC_SR04_NUMS 3



typedef struct hc_sr04_timer {
    uint64_t timer_count_b;     //record the time when the rising edge is triggered
    uint64_t timer_count_e;     //record the time when the falling edge is triggered
} hc_sr04_timer_t;
typedef struct hc_sr04_irq_desc {
    ultra_id_t ultra_id; //we can get struct ultrasonic according to ultra id.
    uint8_t irq_num;     //record stm32 exti line number.
    bool has_bound;      //irq has been bound.
} hc_sr04_irq_desc_t;
typedef struct hc_sr04_resource {
    /*
        HC-SR04 has two pins:ECHO TRIGGER
    */
    GPIO_TypeDef *trigger_gpio_port;
    uint16_t trigger_gpio_pin;
    GPIO_TypeDef *echo_gpio_port;
    uint16_t echo_gpio_pin;
    /*
        use EXTI and a common timer to raise ultrasound's echo
    */
    struct hc_sr04_irq_desc irq_desc;
    struct hc_sr04_timer timer;
} hc_sr04_resource_t;
extern int8_t hc_sr04_init(struct ultrasonic *ultra);
extern int32_t hc_sr04_get_distance(struct ultrasonic *ultra);
extern uint16_t hc_sr04_get_ranging_freq(struct ultrasonic *ultra);
extern hc_sr04_resource_t hc_sr04_res[ultra_max_nums];
#endif
