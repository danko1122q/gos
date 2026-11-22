#ifndef USER_CALC_ENGINE_H
#define USER_CALC_ENGINE_H
typedef long long cnum;
#define CN_SCALE 1000000LL
#define CN_MAX   999999999999999LL

typedef struct {
    cnum acc, cur;
    int op, entering, err, has_acc;
    char entry[24]; int elen, edot;
} Calc;

void calc_init(Calc *c);
void calc_key(Calc *c, int key);
void calc_display(const Calc *c, char *out, int max);
void cn_format(cnum v, char *out, int max);
int  cn_apply(int op, cnum a, cnum b, cnum *r);
int  cn_sqrt(cnum a, cnum *r);
#endif
