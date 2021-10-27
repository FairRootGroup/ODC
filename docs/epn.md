# ODC on the EPN cluster

## Basic

Login to `epn001` as `epn` user:
```bash
ssh epn@epn001
```

Start of the ODC server `odc-grpc-server`:
```bash
nohup systemctl --user start odc &
```

Get status or Stop the ODC server:
```bash
systemctl --user status/stop odc
```

Get list of currently running DDS sessions: 
```bash
dds-session list run
```

Terminate abandoned partition via ODC:
```bash
odc-grpc-client --host epn001:22334
.down --id PARTITION_ID
```

Shutdown abandoned DDS session:
```bash
dds-session stop SESSION_ID
```

**Stop of the `ODC` service doesn't stop `DDS` sessions! On restart `ODC` will automatically reconnect to the running `DDS` sessions.**

## Advanced

### General remarks
Login to head node of EPN `ssh -J USER@lxplus.cern.ch USER@alihlt-gw-prod.cern.ch -p2020`.

From there login to `epn001` as `epn` user - `ssh epn@epn001`.

`epn001` is a server for ODC/DDS.

### Starting of `odc-grpc-server` on EPN cluster

Bash script (`/home/epn/odc/run_odc.sh`) which initializes the environment and starts `odc-grpc-server`:
```bash
#!/usr/bin/env bash

source /home/epn/.bashrc 
#module load ODC InfoLogger
module load O2PDPSuite
export O2_INFOLOGGER_OPTIONS="floodProtection=0"
export DDS_LOCATION=$(dirname $(dirname $(which dds-session)))
dyn="$(which odc-rp-epn) --host epn000:50001 --logdir /home/epn/odc/log --severity dbg --bash \"module load DDS\" --slots 256 --wrkdir \"/tmp/wn_dds\""
rpdyn="epn:${dyn}"
rt="Shutdown:${dyn} --release"
nohup odc-grpc-server --sync --timeout 60 --rp "${rpdyn}" --rt "${rt}" --host "*:22334" --logdir /home/epn/odc/log --severity dbg --infologger --restore epn &

# If script is started via systemd, this file stores the PID of the main process (`PIDFile` in systemd config).
# We want `odc-grpc-server` to be the main process and not `bash`.
dir="/home/epn/.ODC/systemd"
mkdir -p "${dir}"
echo $! > "${dir}/odc.epn.systemd.main.pid"
```

Start odc service via `systemd`:
```bash
nohup systemctl --user start odc &
```

Config file for `systemd` (`~/.config/systemd/user/odc.service`):
```
[Unit]
Description=ODC server
StartLimitIntervalSec=600
StartLimitBurst=5

[Service]
Restart=on-failure
RestartSec=10s
Type=simple
KillMode=process
ExecStart=/home/epn/odc/run_odc.sh
LimitNOFILE=262144
PIDFile=/home/epn/.ODC/systemd/odc.epn.systemd.main.pid

[Install]
WantedBy=multi-user.target
```

Alternatively use `screen` in order to keep `odc-grpc-server` running after user logout:
* Start `screen`: `screen -S odc`
* Detach from `screen`: `Ctrl-A` + `Ctrl-D`.
* Attach to `screen` : `screen -r odc`. 

Alternatevly start `odc-grpc-server` using `systemd-run` (configuration parameters has to be provided as command line options):
```
systemd-run --user --unit=odc ./run_odc.sh
```

Check status, start and stop the service using `systemctl`:
```
systemctl --user status/stop/start odc
```

### ODC monitoring
By default logs are written to `/home/epn/odc/log`.

Use `systemctl --user status odc` to check the current status and running DDS sessions.

`exec.py` script from this repo allows to run arbitrary bash commands on all machines from DDS SSH config file. Sometimes it usefull to check if no shared memory or processes running from previous sessions, i.e.
```
exec.py --cfg hosts-tpc.cfg --cmd 'pgrep o2-*'
exec.py --cfg hosts-tpc.cfg --cmd 'pgrep TfBuilder'
exec.py --cfg hosts-tpc.cfg --cmd 'ls /dev/shm'
exec.py --cfg hosts-tpc.cfg --cmd 'rm /dev/shm/fmq_*'
```

Worker node logs by default are written to `/tmp/wn_dds/XXX/YYY`.

Check for segfaults `abrt-cli list`. Get an `id` and use `abrt-cli info -d <ID>` for a more detailed report. `core_backtrace` and other useful crash info can be found in directory provided in `Directory` field (`/var/spool/abrt/XXX`). Note that one need to use `sudo` to inspect the files in the directory.

#### Kill abandoned partition/session
Check currently active DDS sessions `systemctl --user status odc` or `dds-session list all` or `dds-session list run`.

The simplest way to kill an abandoned session is by using DDS directly `dds-session stop SESSION_ID`. But in this case EPN allocation service will not be notified and allocated nodes are not released.

It's better to use `odc-grpc-client`. Start the client `odc-grpc-client --host epn001:22334`. Type `.down --id PARTITION_ID`. Where `PARTITION_ID` can be obtained either from someone running `AliECS` or by looking it up in the ODC logs, i.e. `grep -B 1 PARTITION_ID *.log`.
