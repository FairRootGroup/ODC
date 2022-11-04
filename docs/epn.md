# ODC on the EPN cluster

## Basic

Login to `epn001` as `epn` user:
```bash
ssh epn@epn001
```

Start/Status/Stop of the ODC server `odc-grpc-server`:
```bash
systemctl --user start/status/stop odc
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

Start/Status/Stop of the ODC service using `systemctl`:
```
systemctl --user start/status/stop odc
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

### Core-based scheduling

Since version 0.73.0 ODC provides support for core-based scheduling when running with Slurm RMS. It translates provided ncores arguments into Slurm's `#SBATCH --cpus-per-task=` per agent group.

In odc-epn-topo one can do `--calib <collection1>:<ncoresx> <collection2>:<ncoresy>`. If the number of cores is ommited, no core-based scheduling will be passed.

Agent groups are created dynamically by odc-epn-topo. There needs to be a mapping to zone definitions of the Slurm plugin of odc-grpc-server (--zones). For this you have to pass --calibzone and --recozone to odc-epn-topo with corresponding zone names.

Example:

odc-grpc-server:
```
odc-grpc-server --sync --host "127.0.0.1:6667" --rp "slurm:/home/rbx/dev/ODC/install/bin/odc-rp-epn-slurm --logdir /home/rbx/odclogs --zones online:4:/home/rbx/sync/work/odc/slurm-online.cfg: calib:2:/home/rbx/sync/work/odc/slurm-calib.cfg:"
```

odc-epn-topo:
```
odc-epn-topo --dd dd-processing.xml --recozone 'online' --reco reco_numa_1.xml reco_numa_2.xml --n 20 --nmin 15 --calibzone 'calib' --calib calib_its.xml:10 calib_mft.xml:20 calib_tof.xml:30 --mon epnstderrlog.xml -o test.xml
```

The above call to odc-epn-topo adds custom requirements to the DDS topology files, that are then used by odc-grpc-server during submission:

```xml
<topology name="topology">
    ...
    <declrequirement name="calib_group1" value="calib1" type="groupname"/>
    <declrequirement name="calib_group2" value="calib2" type="groupname"/>
    <declrequirement name="calib_group3" value="calib3" type="groupname"/>

    <declrequirement name="reco_group1" value="online" type="groupname"/>

    <declrequirement name="odc_ncores_calib_its" value="10" type="custom"/>
    <declrequirement name="odc_ncores_calib_mft" value="20" type="custom"/>
    <declrequirement name="odc_ncores_calib_tof" value="30" type="custom"/>

    <declrequirement name="odc_zone_calib" value="calib" type="custom"/>
    <declrequirement name="odc_zone_reco" value="online" type="custom"/>
    ...
    <declcollection name="RecoCollection">
        <requirements>
            ...
            <name>reco_group1</name>
            <name>odc_zone_reco</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_its">
        <requirements>
            <name>calib_group1</name>
            <name>odc_ncores_calib_its</name>
            <name>odc_zone_calib</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_mft">
        <requirements>
            <name>calib_group2</name>
            <name>odc_ncores_calib_mft</name>
            <name>odc_zone_calib</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_tof">
        <requirements>
            <name>calib_group3</name>
            <name>odc_ncores_calib_tof</name>
            <name>odc_zone_calib</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <main name="main">
        <collection>calib_its</collection>
        <collection>calib_mft</collection>
        <collection>calib_tof</collection>
        <group name="RecoGroup" n="20">
            <collection>RecoCollection</collection>
        </group>
    </main>
</topology>
```

When creating a topology without odc-epn-topo, to use core-based scheduling you need to add custom requirements with odc_ncores_... and odc_zone_... for it to work.

##### Submission Parameters

During submission step, the `n` value for the zone can be ommited to have the submission depend on the ncores requested by the topology.

Examples (valid for ODC >=0.74.0):

| submit zone | submit n | calib collections <br>in the topology | result                                                                                                                                     |
| ----------- | -------- | ------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| calib       | 1        | > 0                                   | schedules the calib collections with core requirements                                                                                     |
| calib       | 1        | 0                                     | submits a request for one calib node, with ncores = slots (num slots is configured in the zone settings). Submitted agents will be unused. |
| calib       |          | > 0                                   | looks at the core requirements and submit/schedule accordingly                                                                             |
| calib       |          | 0                                     | submits nothing to the calib zone                                                                                                          |
|             |          | > 0                                   | fails - complains that it cannot find calib zone                                                                                           |
|             |          | 0                                     | submits nothing                                                                                                                            |

So, for the most flexibility, if you anticipate 0-n collections with core-scheduling in the calib zone, submit should be done with `{ "zone": "calib", "n": 1 }`.
