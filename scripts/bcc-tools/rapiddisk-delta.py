#!/usr/bin/python
# rapiddisk-delta Trace rapiddisk events and deltas between start to finish.
#                 For Linux, uses BCC, eBPF.
#
# Todo: Add operation types (i.e. read, write, discard, etc). I am having
#       a difficult time grabbing the value of bio->bi_opf.
#
# Copyright 2023 - 2025 Petros Koutoupis
# Licensed under the Apache License, Version 2.0 (the "License")
#
# 5-Feb-2023   Petros Koutoupis Created this.

from bcc import BPF
import argparse

# arguments
examples = """examples:
    ./rapiddisk-delta.py           # trace all operations (default)
    ./rapiddisk-delta.py -u 10     # trace operations slower than specified microseconds (default: 0)
    ./rapiddisk-delta.py -p 185    # trace PID 185 only
"""
parser = argparse.ArgumentParser(
    description="Trace rapiddisk operations",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-u", "--min_us", default=0,
    help="trace operations slower than specified microseconds")
parser.add_argument("-p", "--pid",
    help="trace this PID only")
args = parser.parse_args()
min_us = (int(args.min_us) * 1000)
pid = args.pid

# define BPF program
prog = """

#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

struct val_t {
    u64 ts;
};

BPF_HASH(entryinfo, u64, struct val_t);

int trace_entry(struct pt_regs *ctx)
{
    u64 id = bpf_get_current_pid_tgid();
    u32 pid = id >> 32; // PID is higher part
    struct val_t val = {};

    if (FILTER_PID)
        return 0;

    val.ts = bpf_ktime_get_ns();
    entryinfo.update(&id, &val);

    return 0;
}

static u64 delta_return(void)
{
    struct val_t *valp;
    u64 id = bpf_get_current_pid_tgid();

    valp = entryinfo.lookup(&id);
    if (valp == 0) {
        // missed tracing issue or filtered
        return 0;
    }

    // calculate delta
    u64 ts = bpf_ktime_get_ns();
    u64 delta_ns = ts - valp->ts;
    entryinfo.delete(&id);

    return delta_ns;
}

int trace_ioctl_return(struct pt_regs *ctx)
{
    u64 delta_ns = delta_return();
    u32 pid = bpf_get_current_pid_tgid() >> 32; // PID is higher part

    if (FILTER_PID)
        return 0;

    // delta of operation is less than the specified minimum time
    if (delta_ns <= FILTER_US)
        return 0;

    bpf_trace_printk("op: rdsk_ioctl()             delta (ns): %llu\\n", delta_ns);
    return 0;
}

int trace_make_request_return(struct pt_regs *ctx)
{
    u64 delta_ns = delta_return();
    u32 pid = bpf_get_current_pid_tgid() >> 32; // PID is higher part

    if (FILTER_PID)
        return 0;

    // delta of operation is less than the specified minimum time
    if (delta_ns <= FILTER_US)
        return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
    bpf_trace_printk("op: rdsk_submit_bio()        delta (ns): %llu\\n", delta_ns);
#else
    bpf_trace_printk("op: rdsk_make_request()      delta (ns): %llu\\n", delta_ns);
#endif
    return 0;
}

"""

# Starting from Linux 5.9, make_request operations were replaced with submit_bio.
# We are detecing proper functions to trace here.
if BPF.get_kprobe_functions(b'rdsk_submit_bio'):
    make_request_rn = 'rdsk_submit_bio'
else:
    make_request_fn = 'rdsk_make_request'

# code replacement
if args.pid:
    prog = prog.replace('FILTER_PID', 'pid != %s' % pid)
else:
    prog = prog.replace('FILTER_PID', '0')
if min_us == 0:
    prog = prog.replace('FILTER_US', '0')
else:
    prog = prog.replace('FILTER_US', '%s' % str(min_us))

# load BPF program
b = BPF(text=prog)
b.attach_kprobe(event="rdsk_ioctl", fn_name="trace_entry")
b.attach_kprobe(event=make_request_fn, fn_name="trace_entry")
b.attach_kretprobe(event="rdsk_ioctl", fn_name="trace_ioctl_return")
b.attach_kretprobe(event=make_request_fn, fn_name="trace_make_request_return")

# header
print("Tracing rapiddisk operation latency... Hit Ctrl-C to end.")
print("%-18s %-6s %-16s %-6s %s" % ("TIME(s)", "CPU", "COMM", "PID", "MESSAGE"))

# format output
while 1:
    try:
        (task, pid, cpu, flags, ts, msg) = b.trace_fields()
    except ValueError:
        continue
    print("%-18.9f %-6s %-16s %-6d %s" % (ts, cpu, task, pid, msg))
