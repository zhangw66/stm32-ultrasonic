#include <ultra_hc_sr04.h>
#include<tim.h>
#include<stdio.h>
#define pr_err printf
#define pr_info printf
#define pr_debug printf
#define sr04_delay delay_us_poll_systick
#define HC_SR04_MAX_RANGING_FREQ (40U)  //unit:HZ
#define VELOCITY (340.0f) //velocity:340m/s
#define STM32F4_EXTI_NUMS (16u)
#define HC_SR04_RANGING_TIMEOUT (60u)   //unit:ms
#define HC_SR04_RANGING_PERIOD (50u)   //unit:ms

#define gpio_pin_to_exti_line(pin) ({ \
    uint16_t pin_tmp = pin;           \
    uint16_t exti_line_num = 0;       \
    do                                \
    {                                 \
        pin_tmp = pin_tmp >> 1;       \
        exti_line_num++;              \
    } while (pin_tmp);                \
    --exti_line_num;                  \
})
                                            hc_sr04_resource_t hc_sr04_res[ultra_max_nums] = {
    [ultra_id_1] = {
        .trigger_gpio_port = sr04_01_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_01_trigger_pin_Pin,
        .echo_gpio_port = sr04_01_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_01_echo_pin_Pin,
    },
    [ultra_id_2
] = {
        .trigger_gpio_port = sr04_02_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_02_trigger_pin_Pin,
        .echo_gpio_port = sr04_02_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_02_echo_pin_Pin,
    },
    [ultra_id_3
] = {
        .trigger_gpio_port = sr04_03_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_03_trigger_pin_Pin,
        .echo_gpio_port = sr04_03_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_03_echo_pin_Pin,
    },
};
struct hc_sr04_irq_desc *hc_sr04_irq_bind_table[STM32F4_EXTI_NUMS];
static int8_t hc_sr04_bind_irq(struct ultrasonic *ultra)
{
    uint16_t exti_line_num = 0;
    struct ultrasonic *pultra = ultra;
    int8_t ret = 0;
    hc_sr04_resource_t *res = ultra->platform_data;
    if (!res) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    exti_line_num = gpio_pin_to_exti_line(res->echo_gpio_pin);
    if (0 <= exti_line_num && exti_line_num < STM32F4_EXTI_NUMS) {
        res->irq_desc.irq_num = exti_line_num;
        res->irq_desc.ultra_id = pultra->id;
        res->irq_desc.has_bound = true;
        hc_sr04_irq_bind_table[exti_line_num] = &res->irq_desc;
        pr_info("%s[%d]:hc-sr04[%d] has bound to irq[%d]\n", __func__, __LINE__, res->irq_desc.ultra_id, res->irq_desc.irq_num);
    } else {
        pr_err("%s[%d]:exti_line_num is out of range\n", __func__, __LINE__);
        res->irq_desc.has_bound = false;
        ret = -1;
    }
    return ret;
}

static inline void hc_sr04_trigger_process(hc_sr04_resource_t *res)
{
    HAL_GPIO_WritePin(res->trigger_gpio_port, res->trigger_gpio_pin, 1);
    sr04_delay(10);
    HAL_GPIO_WritePin(res->trigger_gpio_port, res->trigger_gpio_pin, 0);
}

static inline struct ultrasonic *hc_sr04_get_dev_by_pin(uint16_t GPIO_Pin)
{
    uint16_t exti_line_num = 0;
    struct hc_sr04_irq_desc *irq_desc = NULL;
    struct ultrasonic *pultra = NULL;
    //get ultra id according to exti line.
    exti_line_num = gpio_pin_to_exti_line(GPIO_Pin);
    if (0 <= exti_line_num && exti_line_num < STM32F4_EXTI_NUMS) {
        irq_desc = hc_sr04_irq_bind_table[exti_line_num];
        if (irq_desc->has_bound) {
            pultra = ultrasonic_get_by_id(irq_desc->ultra_id);
        } else {
            pr_err("%s[%d]:exti_line_num is out of range\n", __func__, __LINE__);
        }
    }
    return pultra;
}

void hc_sr04_irq(uint16_t GPIO_Pin)
{
    struct ultrasonic *pultra = NULL;
    GPIO_PinState pin_state;
    hc_sr04_resource_t *res = NULL;
    ultrasonic_ranging_state_machine_t state = ULTRA_STATE_UNKNOWN;
    pultra = hc_sr04_get_dev_by_pin(GPIO_Pin);
    if (pultra) {
        state = pultra->get_ranging_state(pultra);
        res = (hc_sr04_resource_t *)pultra->platform_data;
        pin_state = HAL_GPIO_ReadPin(res->echo_gpio_port, res->echo_gpio_pin);
        switch (state) {
        case HC_SR04_STATE_RAISE_ECHO_START:
            //raise echo start with a rising edge
            if (GPIO_PIN_SET == pin_state) {
                //clear echo pulse
                pultra->echo_pulse = 0;
                //start timer
                res->timer.timer_count_b = get_timer_time();
                //set ranging state
                pultra->set_ranging_state(pultra, HC_SR04_STATE_RAISE_ECHO_END);
            } else
                pr_err("%s[%d]:get error egde!\n", __func__, __LINE__);
            break;
        case HC_SR04_STATE_RAISE_ECHO_END:
            //raise echo start with a falling edge
            if (GPIO_PIN_RESET == pin_state) {
                //stop timer
                res->timer.timer_count_e = get_timer_time();
                //set echo pulse
                pultra->echo_pulse = res->timer.timer_count_e - res->timer.timer_count_b;
                //set ranging state
                pultra->set_ranging_state(pultra, HC_SR04_STATE_RAISE_CALC_DIS);
            }
            else
                pr_err("%s[%d]:get error egde!\n", __func__, __LINE__);
            break;
        default:
            pr_err("%s[%d]:unexcepted irq!\n", __func__, __LINE__);
            ;
        }
    }
    else
        pr_err("%s[%d]:Can not convert EXTI pin to ultra dev!\n", __func__, __LINE__);
}
void hc_sr04_gpio_init(uint16_t GPIO_Pin)
{

}

int8_t hc_sr04_init(struct ultrasonic *ultra)
{
    hc_sr04_resource_t *res = NULL;
    if (!ultra || !(ultra->platform_data)) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    res = (struct hc_sr04_resource *)ultra->platform_data;
    //setup gpio
	hc_sr04_gpio_init(res->echo_gpio_pin);
    HAL_GPIO_WritePin(res->trigger_gpio_port, res->trigger_gpio_pin, 0);
    HAL_GPIO_WritePin(res->echo_gpio_port, res->echo_gpio_pin, 0);
    //setup timer
    __HAL_TIM_CLEAR_IT(&htim3, TIM_IT_UPDATE);
    HAL_TIM_Base_Start_IT(&htim3);
    /*use global timer*/
    //setup interrupt
    hc_sr04_bind_irq(ultra);
    //init state
    ultra->set_ranging_state(ultra, HC_SR04_STATE_IDLE);
    return 0;
}

int32_t hc_sr04_get_distance(struct ultrasonic *ultra)
{
    int32_t distance = -1; //uint:mm
    uint32_t cur_tick = 0;
    float range, fly_time;
    ultrasonic_ranging_state_machine_t state = ULTRA_STATE_UNKNOWN;
    cur_tick = HAL_GetTick();
	state = ultra->get_ranging_state(ultra);
	//just for delay
	if (cur_tick < ultra->delay && (state == HC_SR04_STATE_IDLE)) {
        //sleep
        return distance;
    }
    switch (state) {
    case HC_SR04_STATE_IDLE:
        //pr_info("<%d>trigger!!\n", ultra->id);
        hc_sr04_trigger_process(ultra->platform_data);
        ultra->set_ranging_state(ultra, HC_SR04_STATE_RAISE_ECHO_START);
        //todo: timeout check.
        ultra->timeout = HAL_GetTick() + HC_SR04_RANGING_TIMEOUT;
        break;
    case HC_SR04_STATE_SEND_ULTRASOUND:
        //do nothing
        break;
    case HC_SR04_STATE_RAISE_CALC_DIS:
        //pr_info("<%d>calc distance!!\n", ultra->id);
        //caculate real distance
        fly_time = (float)ultra->echo_pulse / 1000000.0f;
        range = fly_time * VELOCITY / 2;
		distance = (int32_t)(range * 1000.0f);
        pr_debug("<%d>range:%f\n", ultra->id, range);
        //Start a new round of measurement
        ultra->set_ranging_state(ultra, HC_SR04_STATE_IDLE);
		ultra->delay = HAL_GetTick() + HC_SR04_RANGING_PERIOD;
        break;
    default:;
    }
	//just for timeout
    if (cur_tick > ultra->timeout && (state != HC_SR04_STATE_RAISE_CALC_DIS)) {
        pr_err("%s[%d]: HC-SR04%d range timeout!\n", __func__, __LINE__, ultra->id);
        ultra->set_ranging_state(ultra, HC_SR04_STATE_IDLE);
    }
    return distance;
}

uint16_t hc_sr04_get_ranging_freq(struct ultrasonic *ultra)
{
    uint16_t freq = 0;
    freq = HC_SR04_MAX_RANGING_FREQ;
    ultra->max_ranging_freq = freq;
    return freq;
}
