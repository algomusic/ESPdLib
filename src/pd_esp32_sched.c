/*
 * pd_esp32_sched.c - Pthread-free Pd scheduler for ESP32
 *
 * Replaces m_sched.c which uses pthreads (causing pthread_self crash on ESP32).
 * Based on the proven ESPD reference implementation's scheduler.
 *
 * Provides: clock_new, clock_set, clock_delay, clock_unset, clock_free,
 *           clock_setunit, clock_getlogicaltime, clock_getsystime,
 *           clock_gettimesince, clock_gettimesincewithunits,
 *           clock_getsystimeafter, sched_tick, sched_init, sys_quit
 */

#include "pd_build_defines.h"
#include "pure_data/src/m_pd.h"
#include "pure_data/src/s_stuff.h"

#define TIMEUNITPERMSEC (32. * 441.)
#define TIMEUNITPERSECOND (TIMEUNITPERMSEC * 1000.)

void dsp_tick(void);
void outlet_setstacklim(void);

static int sched_diddsp;
int sys_quit = 0;

void sched_init(void) {}

typedef void (*t_clockmethod)(void *client);

struct _clock
{
    double c_settime;       /* in TIMEUNITS; <0 if unset */
    void *c_owner;
    t_clockmethod c_fn;
    struct _clock *c_next;
    t_float c_unit;         /* >0 if in TIMEUNITS; <0 if in samples */
};

t_clock *clock_new(void *owner, t_method fn)
{
    t_clock *x = (t_clock *)getbytes(sizeof *x);
    x->c_settime = -1;
    x->c_owner = owner;
    x->c_fn = (t_clockmethod)fn;
    x->c_next = 0;
    x->c_unit = TIMEUNITPERMSEC;
    return (x);
}

void clock_unset(t_clock *x)
{
    if (x->c_settime >= 0)
    {
        if (x == pd_this->pd_clock_setlist)
            pd_this->pd_clock_setlist = x->c_next;
        else
        {
            t_clock *x2 = pd_this->pd_clock_setlist;
            while (x2->c_next != x) x2 = x2->c_next;
            x2->c_next = x->c_next;
        }
        x->c_settime = -1;
    }
}

void clock_set(t_clock *x, double setticks)
{
    if (setticks < pd_this->pd_systime) setticks = pd_this->pd_systime;
    clock_unset(x);
    x->c_settime = setticks;
    if (pd_this->pd_clock_setlist &&
        pd_this->pd_clock_setlist->c_settime <= setticks)
    {
        t_clock *cbefore, *cafter;
        for (cbefore = pd_this->pd_clock_setlist,
            cafter = pd_this->pd_clock_setlist->c_next;
                cbefore; cbefore = cafter, cafter = cbefore->c_next)
        {
            if (!cafter || cafter->c_settime > setticks)
            {
                cbefore->c_next = x;
                x->c_next = cafter;
                return;
            }
        }
    }
    else x->c_next = pd_this->pd_clock_setlist, pd_this->pd_clock_setlist = x;
}

void clock_delay(t_clock *x, double delaytime)
{
    clock_set(x, (x->c_unit > 0 ?
        pd_this->pd_systime + x->c_unit * delaytime :
            pd_this->pd_systime -
                (x->c_unit*(TIMEUNITPERSECOND/STUFF->st_dacsr)) * delaytime));
}

void clock_setunit(t_clock *x, double timeunit, int sampflag)
{
    double timeleft;
    if (timeunit <= 0)
        timeunit = 1;
    if ((sampflag && (timeunit == -x->c_unit)) ||
        (!sampflag && (timeunit == x->c_unit * TIMEUNITPERMSEC)))
            return;
    timeleft = (x->c_settime < 0 ? -1 :
        (x->c_settime - pd_this->pd_systime)/((x->c_unit > 0)? x->c_unit :
            (x->c_unit*(TIMEUNITPERSECOND/STUFF->st_dacsr))));
    if (sampflag)
        x->c_unit = -timeunit;
    else x->c_unit = timeunit * TIMEUNITPERMSEC;
    if (timeleft >= 0)
        clock_delay(x, timeleft);
}

double clock_getlogicaltime(void)
{
    return (pd_this->pd_systime);
}

double clock_getsystime(void)
{
    return (pd_this->pd_systime);
}

double clock_gettimesince(double prevsystime)
{
    return ((pd_this->pd_systime - prevsystime)/TIMEUNITPERMSEC);
}

double clock_gettimesincewithunits(double prevsystime,
    double units, int sampflag)
{
    if (sampflag)
        return ((pd_this->pd_systime - prevsystime)/
            ((TIMEUNITPERSECOND/STUFF->st_dacsr)*units));
    else return ((pd_this->pd_systime - prevsystime)/(TIMEUNITPERMSEC*units));
}

double clock_getsystimeafter(double delaytime)
{
    return (pd_this->pd_systime + TIMEUNITPERMSEC * delaytime);
}

void clock_free(t_clock *x)
{
    clock_unset(x);
    freebytes(x, sizeof *x);
}

/* Take the scheduler forward one DSP tick, processing clock timeouts */
void sched_tick(void)
{
    double next_sys_time = pd_this->pd_systime +
        (STUFF->st_schedblocksize/STUFF->st_dacsr) * TIMEUNITPERSECOND;
    int countdown = 5000;
    while (pd_this->pd_clock_setlist &&
        pd_this->pd_clock_setlist->c_settime < next_sys_time)
    {
        t_clock *c = pd_this->pd_clock_setlist;
        pd_this->pd_systime = c->c_settime;
        clock_unset(pd_this->pd_clock_setlist);
        outlet_setstacklim();
        (*c->c_fn)(c->c_owner);
        if (!countdown--)
        {
            countdown = 5000;
            sys_pollgui();
        }
        if (sys_quit)
            return;
    }
    pd_this->pd_systime = next_sys_time;
    dsp_tick();
    sched_diddsp++;
}
