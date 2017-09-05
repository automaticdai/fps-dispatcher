///////////////////////////////////////////////////////////////////////////////
#include "dispatcher.h"

/* Task Data Structure */
typedef void(*callback)(void);

typedef enum { ready, running, pending, waiting, deleted } enum_task_status;
char *task_status_literal[] = {
    { "ready" },
    { "running" },
    { "pending" },
    { "waiting" },
    { "deleted" }
};

typedef enum { fps, edf } enum_scheduling_policy;

class Task
{
public:
    int id_;
    int C_;
    int D_;
    int T_;
    int R_;
    int c_; // computation time countdown
    int d_; // deadline countdown
    int r_; // next release countdown
    int cnt_; // release count

    enum_task_status status_;
    callback onstart_;
    callback onfinish_;

public:
    Task(int id = 0, int Ci = 0, int Ti = 0, int Di = 0, int Ri = 0) :
        id_(id),
        C_(Ci),
        T_(Ti),
        D_(Di),
        R_(Ri)
    {
        if (Di == 0) {
            D_ = Ti;
        }
        status_ = deleted;
        cnt_ = 0;
        onstart_ = NULL;
        onfinish_ = NULL;
    }

    ~Task() { ; }

    void on_task_ready(void) {
        c_ = C_;
        d_ = D_;
        r_ = T_;
        cnt_++;
    }

    void on_task_start(void) {
        cout << "t:" << kernel_cnt << ", r" << '(' << id_ << ',' << cnt_ << ')' << '\n';
        if (onstart_ != NULL) {
            onstart_();
        }
    }


    void on_task_finish(void) {
        cout << "t:" << kernel_cnt << ", f" << '(' << id_ << ',' << cnt_ << ')' << '\n';
        if (onfinish_ != NULL) {
            onfinish_();
        }
    }

    void set_onstart_hook(callback onstart)
    {
        onstart_ = onstart;
    }

    void set_onfinish_hook(callback onfinish)
    {
        onfinish_ = onfinish;
    }

    void repr()
    {
        std::cout << id_ << ":" << task_status_literal[status_] << " | " << c_ << " | " << d_<< " | " << r_ << "\n";
    }
};
typedef class Task CTask;



///////////////////////////////////////////////////////////////////////////////
/* Scheduler Kernel Variables */
#define TASK_MAX_NUM        (TASK_NUMBERS)
#define IDLE_TASK_IDX       (TASK_MAX_NUM)

CTask TCB[TASK_MAX_NUM];
//enum_task_status task_status_list[TASK_MAX_NUM];

//bool ready_q[TASK_MAX_NUM];
//bool pending_q[TASK_MAX_NUM];

long kernel_cnt;
long idle_cnt;
int  tcb_running_id;

/* Scheduler Kernel Functions */
void afbs_initilize(enum_scheduling_policy sp);

void afbs_create_task(CTask t, callback task_main, callback on_start, callback on_finish);
void afbs_delete_task(int task_id);
void afbs_create_job(CTask j, int prio, callback job_main, callback on_start, callback on_finish);
void afbs_delete_job(int job_id);

void afbs_update(void);
void afbs_schedule(void);
void afbs_run(void);
void afbs_idle(void);
void afbs_dump_information(void);

/// function implementation
void afbs_initilize(enum_scheduling_policy sp)
{
    int i = 0;
    for (; i < TASK_MAX_NUM; i++) {
        TCB[i].id_ = i;
        TCB[i].status_ = deleted;
    }
    tcb_running_id = IDLE_TASK_IDX;
}

void afbs_create_task(CTask t, callback task_main, callback on_start, callback on_finish)
{
    if (t.R_ == 0) {
        t.status_ = ready;
        t.on_task_ready();
    }
    else {
        t.r_ = t.R_;
        t.status_ = waiting;
    }

    t.set_onstart_hook(on_start);
    t.set_onfinish_hook(on_finish);

    TCB[t.id_] = t;
}

void afbs_delete_task(int task_id)
{
    ;
}

void afbs_create_job(CTask j, int prio, callback job_main, callback on_start, callback on_finish)
{
    ;
}

void afbs_delete_job(int job_id)
{
    ;
}


int step_count = 0;
double alpha = 0;
void afbs_update(void)
{
    kernel_cnt++;

    for (int i = 0; i < TASK_MAX_NUM; i++) {
        if (TCB[i].status_ != deleted) {
            if (--TCB[i].r_ == 0) {
                TCB[i].status_ = ready;
                TCB[i].on_task_ready();
            }
        }
    }

    // if current task is finished, set current task to IDLE
    if (TCB[tcb_running_id].c_ == 0)
    {
        tcb_running_id = IDLE_TASK_IDX;
    }

    // feedback scheduling handler
    // a little bit hacking
    if (kernel_cnt % (int)(AFBS_SAMPLING_PERIOD / KERNEL_TIME) == 0) {
        // state feedback
        //TCB[1].T_ = floor(TASK_1_PERIOD * pow(exp(1.0), (-10.0 * error[1])) + TASK_1_PERIOD);

        // Dual-period model
        //if (error[2] > 0.3) {
        if (step_count <= alpha * (int)(T_GAMMA / AFBS_SAMPLING_PERIOD)) {
            /*
            if (TCB[2].T_ != 20) {
                TCB[2].r_ = 1;
            }*/
            TCB[0].T_ = TASK_1_PERIOD;
        }
        else {
            TCB[0].T_ = TASK_2_PERIOD;

            /* event detection */
            if (step_count > (int)(T_GAMMA / AFBS_SAMPLING_PERIOD)
                && abs(error[0]) > TRIG_ERROR_THRESHOLD) {
                TCB[0].T_ = TASK_1_PERIOD;
                step_count = 0;
            }

        }
        step_count += 1;
    }

}

void  afbs_schedule(void) {
    int task_to_be_scheduled = IDLE_TASK_IDX;

    for (int i = 0; i < TASK_MAX_NUM; i++)
    {
        if ((TCB[i].status_ == ready) || (TCB[i].status_ == pending)) {
            task_to_be_scheduled = i;
            break;
        }
    }

    if ((task_to_be_scheduled != IDLE_TASK_IDX)) {
        for (int i = task_to_be_scheduled + 1; i < TASK_MAX_NUM; i++) {
            if (TCB[i].status_ == ready) {
                TCB[i].status_ = pending;
            }
        }
    }

    // task scheduled hook
    if ((task_to_be_scheduled != tcb_running_id) &&
       (TCB[task_to_be_scheduled].c_ == TCB[task_to_be_scheduled].C_)) {
        TCB[task_to_be_scheduled].on_task_start();
    }

    TCB[task_to_be_scheduled].status_ = ready;
    tcb_running_id = task_to_be_scheduled;
}

void afbs_run(void) {
    if (tcb_running_id != IDLE_TASK_IDX) {
        if (--TCB[tcb_running_id].c_ == 0) {
            TCB[tcb_running_id].status_ = waiting;
            TCB[tcb_running_id].on_task_finish();
            //cout << 'f';
        }
    }
    else {
        afbs_idle();
    }
}

void afbs_idle(void)
{
    idle_cnt++;
}

void afbs_dump_information(void)
{
    cout << "t: " << kernel_cnt << '\n';
    cout << "Current Task: " << tcb_running_id;
    cout << '\n';
    cout << "Task TCB: \n";
    for (int i = 0; i < TASK_MAX_NUM; i++) {
        TCB[i].repr();
    }
    cout << '\n';
}

/*************************************************************************/
