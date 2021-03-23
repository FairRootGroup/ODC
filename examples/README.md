# ODC Examples

ODC provides several examples of [DDS topologies](http://dds.gsi.de/doc/nightly/topology.html) which are installed in `INSTALL_DIR/share/odc`.

## Run locally

Source DDS environment script before starting ODC server:
```
> cd DDS_INSTALL_DIR && source DDS_env.sh
```

Start ODC gRPC server:
```
> odc-grpc-server
```

Start ODC gRPC client:
```
> odc-grpc-client
```

Alternatively, start the CLI server:
```
> odc-cli-server
```

By default this example uses [localhost plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html#localhost-plugin) of [DDS](https://github.com/FairRootGroup/DDS) and three topologies which are installed in `INSTALL_DIR/share/odc/`:

* `ex-dds-topology-infinite.xml` - initial topology with 12 FairMQ devices;
* `ex-dds-topology-infinite-up.xml` - upscaled topololy with 36 FairMQ devices;
* `ex-dds-topology-infinite-down.xml` - downscaled topology with 24 FairMQ devices.

## Run on ALICE EPN cluster

Installation of ODC and its dependencies using [aliBuild](https://github.com/alisw/alibuild):

```
> mkdir -p INSTALL_DIR && cd INSTALL_DIR
> git clone https://github.com/alisw/alidist.git
> aliBuild --default o2-dataflow build ODC
```

For this example we use [SSH plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html#ssh-plugin) of [DDS](https://github.com/FairRootGroup/DDS) which requires a SSH configuration file.

Create file `hosts.cfg` with the following content:

```
@bash_begin@
cd INSTALL_DIR && eval `alienv --no-refresh load ODC/latest-o2-dataflow` && cd -
@bash_end@

wn_epn002, epn002, , WRK_DIR,  12
wn_epn003, epn003, , WRK_DIR,  12
wn_epn004, epn004, , WRK_DIR,  12
```

Replace `INSTALL_DIR` and `WRK_DIR` with the real directory names. Important prerequisite: working directories `WRK_DIR` from SSH config file must exist. We recommend to use the local storage of the machine (i.e. `/tmp`) for the working directories.

Open two terminal windows. In each of the terminal first initialize the environment:
```
> cd INSTALL_DIR && alienv enter ODC/latest-o2-dataflow
> export LC_ALL=C; unset LANGUAGE
> cd sw/slc8_x86-64/DDS/latest && source DDS_env.sh
```

Start ODC gRPC server:
```
> odc-grpc-server
```

Start ODC gRPC client:
```
> odc-grpc-client
```

### Data Distribution example
For a Data Distribution example create a topology file `ex-dd-topology.xml` with the following content:
```
<topology name="o2-dd">
   <declrequirement name="TfBuilderRequirement" type="maxinstances" value="1"/>
   <decltask name="TfBuilderTask">
       <exe reachable="true">tfbuilder.sh</exe>
       <requirements>
           <name>TfBuilderRequirement</name>
       </requirements>
   </decltask>
   <main name="main">
       <group name="TfBuilderGroup" n="3">
           <task>TfBuilderTask</task>
       </group>
   </main>
</topology>
```
`tfbuilder.sh` is a bash script which initializes the environment and starts `TfBuilder`:
```
#!/usr/bin/env bash
module load DataDistribution/v0.9.7-5
TfBuilder -P dds --transport shmem --session default --io-threads=24 --severity debug --id tfb --tf-memory-size=$((128 * 1024)) --discovery-net-if=ib0 --discovery-endpoint=http://epn000-ib:8500 --stand-alone
```
Start ODC gRPC server:
```
odc-grpc-server --host "*:PORT"
```
Start ODC gRPC client:
```
odc-grpc-client --host "HOST:PORT"
```
Use the following sequence of commands:
```
.init
.submit -p odc-rp-same -r "<rms>ssh</rms><configFile>hosts.cfg</configFile><requiredSlots>36</requiredSlots>"
.activate --topo ex-dd-topology.xml 
.config
.start
.stop
.reset
.term
.down
.quit
```
We use `hosts.cfg` file from previous example and built-in `odc-rp-same` resource plugin. 

## Run on GSI Virgo cluster

Connect to Virgo cluster at GSI:
```
ssh -J $USER@lxpool.gsi.de virgo-centos7.hpc.gsi.de
```
ODC and its dependencies are installed in `/cvmfs/cbm.gsi.de/centos7/opt/linux-centos7-x86_64/`. Please use the latest available release.

**Important note:** By default `http_proxy` is set on Virgo submitter nodes. We need to explicitly disable the use of HTTP proxy by exporting `no_grpc_proxy`. See [gRPC environment variables](https://grpc.github.io/grpc/cpp/md_doc_environment_variables.html) for details.
```
export no_grpc_proxy={ODC_SERVER_HOSTNAME}
```

Set `$HOME` to your `lustre` home directory and set locale:
```
> export HOME=/lustre/GROUP/LOGIN
> export LC_ALL=C; unset LANGUAGE
```

For this example we use [Slurm plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html#slurm-plugin) of [DDS](https://github.com/FairRootGroup/DDS). In general no additional configuration is required for Slurm. But if neccessary one can create a configuration file with additional parameters. For example, a `slurm.cfg` file which sets the account and partition to `cbm_online`:

```
#SBATCH --account=cbm_online
#SBATCH --partition=cbm_online
```

Start ODC gRPC server:
```
> odc-grpc-server --host "*:PORT"
```
The number of agents (3) and slots (12) is required to test default topoloies of ODC. These numbers has to be adjusted if you want to test your own topology. 

Start ODC gRPC client:
```
> odc-grpc-client --host "{ODC_SERVER_HOSTNAME}:PORT"
```
`{ODC_SERVER_HOSTNAME}` has to be replace with the real hostname. Port can be change.

Submit request looks like:
```
.submit -p odc-rp-same -r "<rms>slurm</rms><configFile>PATH_TO_CFG/slurm.cfg</configFile><agents>3</agents><slots>12</slots><requiredSlots>36</requiredSlots>"
```

## Standard sequence of requests
```
.init
.submit -p odc-rp-same -r "<rms>ssh</rms><configFile>hosts.cfg</configFile><requiredSlots>36</requiredSlots>"
.activate --topo INSTALL_DIR/share/odc/ex-dds-topology-infinite.xml
.config
.start
.stop
.upscale --topo INSTALL_DIR/share/odc/ex-dds-topology-infinite-up.xml
.start
.stop
.downscale --topo INSTALL_DIR/share/odc/ex-dds-topology-infinite-down.xml
.start
.stop
.reset
.term
.down
.quit
```

The default sequence of request can also be executed in batch mode using `--batch` option. The sequence of commands can be changed via `--cmds` option.

## Create a DDS topology

[The DDS topology example](src/odc-topo.cpp) shows how to create a topology XML file using [DDS topology APIs](https://github.com/FairRootGroup/DDS/tree/master/dds-topology-lib/src). In the example we combine two topologies [1](ex-dpl-topology.xml) and [2](ex-dd-topology.xml) into a single XML file. The first topology is an example of the DPL export to DDS. The second one is an example of TfBuilder task declaration. 

We create a "EPNGroup" group and add it into the main group of the topology. Set required number of  collections. Read DPL collection from XML file and add it to "EPNGroup" group. Add TfBuilder task, which is read from XML file, to "EPNGroup". Save the topology to a file.
