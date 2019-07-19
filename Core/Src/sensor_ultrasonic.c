#include <sensor_ultrasonic.h>
#include <stdio.h>
#include <tim.h>
//workaround
#define pr_err printf
#define sr04_delay delay_us_poll_systick
#define HC_SR04_MAX_RANGING_FREQ (40U)
#define VELOCITY (340.0f) //velocity:340m/s
//hc_sr04_ranging_state_machine_t hc_sr04_ranging_state_machine;
hc_sr04_resource_t hc_sr04_res[ultra_max_nums] = {
    [ultra_id_3] = {
        .trigger_gpio_port = sr04_01_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_01_trigger_pin_Pin,
        .echo_gpio_port = sr04_01_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_01_echo_pin_Pin,
    },
    [ultra_id_1] = {
        .trigger_gpio_port = sr04_03_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_03_trigger_pin_Pin,
        .echo_gpio_port = sr04_03_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_03_echo_pin_Pin,
    },
    [ultra_id_2] = {
        .trigger_gpio_port = sr04_04_trigger_pin_GPIO_Port,
        .trigger_gpio_pin = sr04_04_trigger_pin_Pin,
        .echo_gpio_port = sr04_04_echo_pin_GPIO_Port,
        .echo_gpio_pin = sr04_04_echo_pin_Pin,
    },
};
struct ultrasonic ultrasonic_table[ultra_max_nums] = {
    [ultra_id_1] = {
    },
};
#if 0
#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({             \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
})
#endif
#define STM32F4_EXTI_NUMS (16)
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
        printf("%s[%d]:hc-sr04[%d] has bound to irq[%d]\n", __func__, __LINE__, res->irq_desc.ultra_id, res->irq_desc.irq_num);
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
    if (0 <= exti_line_num && exti_line_num < STM32F4_EXTI_NUMS)
    {
        irq_desc = hc_sr04_irq_bind_table[exti_line_num];
        if (irq_desc->has_bound) {
            pultra = ultrasonic_get_by_id(irq_desc->ultra_id);
        } else {
            pr_err("%s[%d]:exti_line_num is out of range\n", __func__, __LINE__);
        }
    }
    return pultra;
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
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
            } else
                pr_err("%s[%d]:get error egde!\n", __func__, __LINE__);
            break;
        default:
            pr_err("%s[%d]:unexcepted irq!\n", __func__, __LINE__);
            ;
        }
    } else
        pr_err("%s[%d]:Can not convert EXTI pin to ultra dev!\n", __func__, __LINE__);
}

int8_t hc_sr04_init(struct ultrasonic *ultra)
{
    hc_sr04_resource_t *res;
    if (!ultra || !(ultra->platform_data)) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    res = (struct hc_sr04_resource *)ultra->platform_data;
    //init gpio
    HAL_GPIO_WritePin(res->trigger_gpio_port, res->trigger_gpio_pin, 0);
    HAL_GPIO_WritePin(res->echo_gpio_port, res->echo_gpio_pin, 0);
    //init timer
    __HAL_TIM_CLEAR_IT(&htim3, TIM_IT_UPDATE);
    HAL_TIM_Base_Start_IT(&htim3);
    //enbale echo pin irq line
    hc_sr04_bind_irq(ultra);
    //init state
    ultra->set_ranging_state(ultra, HC_SR04_STATE_IDLE);
    return 0;
}
int16_t hc_sr04_get_distance(struct ultrasonic *ultra)
{
    int16_t distance = -1; //uint:cm
    float range, fly_time;
    ultrasonic_ranging_state_machine_t state = ULTRA_STATE_UNKNOWN;
    state = ultra->get_ranging_state(ultra);
    switch (state) {
    case HC_SR04_STATE_IDLE:
        printf("<%d>trigger!!\n", ultra->id);
        hc_sr04_trigger_process(ultra->platform_data);
        ultra->set_ranging_state(ultra, HC_SR04_STATE_RAISE_ECHO_START);
        //todo: timeout check.
        break;
    case HC_SR04_STATE_SEND_ULTRASOUND:
        //do nothing
        break;
    case HC_SR04_STATE_RAISE_CALC_DIS:
        printf("<%d>calc distance!!\n", ultra->id);
        //caculate real distance
        fly_time = (float)ultra->echo_pulse / 1000000.0f;
        range = fly_time * VELOCITY / 2;
        printf("<%d>range:%f\n", ultra->id, range);
        //Start a new round of measurement
        ultra->set_ranging_state(ultra, HC_SR04_STATE_IDLE);
        break;
    default:;
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



int8_t ultrasonic_install_hook(struct ultrasonic *ultra,
                               ultrasonic_init_t init,
                               ultrasonic_get_distance_t get_distance,
                               ultrasonic_get_ranging_freq_t get_ranging_freq)
{
    if (!ultra || !get_distance) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    if (init)
        ultra->init = init;
    if (get_distance)
        ultra->get_distance = get_distance;
    if (get_ranging_freq)
        ultra->get_ranging_freq = get_ranging_freq;
    return 0;
}
int8_t ultrasonic_set_id(struct ultrasonic *ultra, ultra_id_t id,const char *name)
{
    int8_t ret = 0;
    if (!ultra) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    if (id >= 0 && id < ultra_max_nums)
        ultra->id = id;
    else {
        pr_err("%s[%d]: id<%d> is invalid!\n", __func__, __LINE__, id);
        ret = -1;
    }
    if (name)
        ultra->hardware_name = name;
    else
        ultra->hardware_name = NULL;
    return ret;
}
struct ultrasonic *ultrasonic_get_by_id(uint16_t id)
{
    struct ultrasonic *ultra = NULL;
    if (id < 0 || id >= ultra_max_nums) {
        pr_err("%s[%d]: ultra id<%u> is out of range\n", __func__, __LINE__, id);
        return NULL;
    }
    if ((ultrasonic_table[id].id == id)
        && ultrasonic_table[id].platform_data 
        && ultrasonic_table[id].get_distance) {
        //pr_err("%s[%d]: Get ultra-%d successful!!\n", __func__, __LINE__, id);
        ultra = &ultrasonic_table[id];
    } else
        pr_err("%s[%d]: ultra-%d is invalid,please setup first!!\n", __func__, __LINE__, id);
    return ultra;
}
int8_t ultrasonic_set_platdata(struct ultrasonic *ultra, void* data)
{
    int8_t ret = 0;
    if (!ultra || !data)
    {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    ultra->platform_data = data;
    return ret;
}
ultrasonic_ranging_state_machine_t ultra_get_ranging_state(struct ultrasonic *ultra)
{
    return ultra->ranging_state;
}
int8_t ultra_set_ranging_state(struct ultrasonic *ultra, ultrasonic_ranging_state_machine_t state)
{
    ultra->ranging_state = state;
    return 0; 
}

int8_t ultrasonic_config_state_machine(struct ultrasonic *ultra, set_ranging_state_t set_func, get_ranging_state_t get_func)
{
    int8_t ret = 0;
    if (!ultra)
    {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    ultra->set_ranging_state = (!set_func) ? ultra_set_ranging_state :
        set_func;
        ultra->get_ranging_state = (!get_func) ? ultra_get_ranging_state : get_func;
        return ret;
}

int8_t ultrasonic_setup(ultra_type_t type, uint16_t ultra_dev_nums)
{
    uint16_t ultra_nums;
    if (type < 0 || type >= ultra_type_nums) {
        pr_err("%s[%d]: ultra type<%u> is unknown\n", __func__, __LINE__, type);
        return -1;
    }
    //TODO:get how many ultrasonic we have in meissa board.
    ultra_nums = ultra_dev_nums;
    printf("%s:we have %d %s ultrasonic\n", __func__, ultra_nums, get_ultra_type_str(type));
    //Set the corresponding callback function according to different sensor types
    for (int i = 0; i < ultra_nums; i++) {
        ultrasonic_set_id(&ultrasonic_table[i], i, get_ultra_type_str(type));
        ultrasonic_config_state_machine(&ultrasonic_table[i], NULL, NULL);
        ultrasonic_install_hook(&ultrasonic_table[i], hc_sr04_init, hc_sr04_get_distance, hc_sr04_get_ranging_freq);
        ultrasonic_set_platdata(&ultrasonic_table[i], &hc_sr04_res[i]);
    }
    return 0;
}
