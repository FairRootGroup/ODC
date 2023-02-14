For resource plugin basics refer to [docs/rp.md](../../docs.rp.md).

## odc-rp-epn-slurm resource plugin

This plugin transforms incoming resource descriptions from AliECS into [DDS](https://dds.gsi.de) Slurm [RMS plugin configuration](http://dds.gsi.de/doc/nightly/RMS-plugins.html#slurm-plugin).

As described in [ODC resource plugin docs](../../docs.rp.md), a typical resource request from AliECS looks like:

```JSON
{
   "zone":"online",
   "n":10
}
```

or an array of such objects. `odc-rp-epn-slurm` plugin maps each `zone` from the request to a Slurm submit configuration, and each `n` to a number of DDS agents (which typicaly translates into number of nodes, or number of DDS Collections of certain type).

### Configuration

Configuration for each zone should be specified as a parameter when the plugin is registered with `odc-grpc-server` to allow for the Slurm configuration to be extended. Example:

```
--zones online:8:/home/user/slurm-online.cfg calib:8:/home/user/slurm-calib.cfg
```
`--zones` takes any number of string arguments that translate into zone configurations in the `<name>:<numSlots>:<slurmCfgPath>` format. The arguments are:
- **name** - the name of the zone
- **numSlots** - translates to `--slots` of DDS submit command (slots per agent), which for Slurm translates to `#SBATCH --cpus-per-task <numSlots>` entry in the sbatch file. Should be equal or higher for the number of deployed tasks for this zone.
- **slurmCfgPath** - A full path to a Slurm config file, which is appended to the [slurm sbatch file](https://slurm.schedmd.com/sbatch.html). Here you can specify any additional Slurm parameters that you want to use for this zone.

For DDS tasks and collections to be deployed on the configured zones, they have to specify the `groupname` requirement with the zone name as its value (see an example below).

### Example Usage

1. Start odc-grpc-server:
```bash
odc-grpc-server --host "127.0.0.1:6667" --logdir /home/user/odclogs --rp "slurm:/home/user/dev/ODC/install/bin/odc-rp-epn-slurm --logdir /home/user/odclogs --zones online:8:/home/user/slurm-online.cfg calib:8:/home/user/slurm-calib.cfg"
```
This starts the `odc-grpc-server` with the Slurm plugin, configured to accept 2 zones: `online` and `calib`, with corresponding Slurm config files and number of slots per Agent.
The corresponding config files can have all sorts of Slurm sbatch parameters, but in this particular example we have:
```bash
> cat slurm-online.cfg
#SBATCH --time=00:30:00
#SBATCH --account "aliecs"
#SBATCH --partition "online"
> cat slurm-calib.cfg
#SBATCH --time=00:30:00
#SBATCH --account "aliecs"
#SBATCH --partition "calib"
```
Thus, zone `online` will be submitted to the Slurm `online` partition and zone `calib` to the `calib` partition.

2. Start odc-grpc-client:
```
odc-grpc-client --host 127.0.0.1:6667 --logdir /home/user/odclogs/
```
This starts the `odc-grpc-client`. In a real life scenario this client would be replaced by AliECS, but it talks the same gRPC protocol with the server.

3. Issue following commands in the `odc-grpc-client`:
```
.init
```
This starts DDS commander.
```
.submit -p slurm -r "[{ \"zone\":\"online\", \"n\":1 }, { \"zone\":\"calib\", \"n\":1 }]"
```
This performs two DDS submissions via Slurm. First one submits with the configuration for the `online` zone, and second for the `calib` zone.
```
.activate --topo "/home/user/topo.xml"
```
This activates the given DDS topology.
For tasks and collections from this topology to be deployed on the configured zones, they have to be specified with `groupname` requirements as follows:
```xml
    <declrequirement name="reqCalib" type="groupname" value="calib"/>
    <declrequirement name="reqOnline" type="groupname" value="online"/>

    <decltask name="Task1">
        <exe>task1.exe</exe>
    </decltask>

    <decltask name="Task2">
        <exe>task2.exe</exe>
    </decltask>

    <declcollection name="Col1">
        <requirements>
            <name>reqCalib</name>
        </requirements>
       <tasks>
           <name>Task1</name>
       </tasks>
    </declcollection>

    <declcollection name="Col2">
        <requirements>
            <name>reqOnline</name>
        </requirements>
       <tasks>
           <name n="8">Task2</name>
       </tasks>
    </declcollection>

    <main name="main">
        <group name="Group" n="1">
            <collection>Col1</collection>
            <collection>Col2</collection>
        </group>
    </main>
```
Collection `Col1` will be deployed on the `calib` Slurm partition, and `Col2` on the `online` partition.

### core-based scheduling

Since version 0.73.0 ODC provides support for core-based scheduling. It translates provided number of cores parameters into Slurm's `#SBATCH --cpus-per-task=` per agent group.

In odc-epn-topo you can now do `--calib <collection1>:<ncoresx> <collection2>:<ncoresy>`. If the number of cores is ommited, it would be get scheduled as before.

The agent groups are created dynamically by odc-epn-topo. Thus the `--calibgroup` and `--recogroup` arguments are no longer present, you'll have to remove them for ODC 0.73.0. But there still needs to be a mapping to zone definitions of the Slurm plugin of odc-grpc-server (`--zones`). So, now you have to pass `--calibzone` and `--recozone` to odc-epn-topo with corresponding zone names.

Example:

odc-grpc-server:

```
odc-grpc-server --sync --host "127.0.0.1:6667" --rp "slurm:/home/rbx/dev/ODC/install/bin/odc-rp-epn-slurm --logdir /home/rbx/odclogs --zones online:4:/home/rbx/sync/work/odc/slurm-online.cfg: calib:2:/home/rbx/sync/work/odc/slurm-calib.cfg:"
```

odc-epn-topo:

```
odc-epn-topo --dd dd-processing.xml --recozone 'online' --reco reco_numa_1.xml reco_numa_2.xml --n 20 --nmin 15 --calibzone 'calib' --calib calib_its.xml:10 calib_mft.xml:10 calib_tof.xml:20 --prependexe "module load O2PDPSuite/epn-20220215-1 2>&1 ; " --mon epnstderrlog.xml -o test.xml
```

The above call to odc-epn-topo adds custom requirements to the DDS topology files, that are then used by odc-grpc-server during submission:

```xml
<topology name="topology">
    ...
    <declrequirement name="calib_g1" type="groupname" value="calib1"/>
    <declrequirement name="calib_g2" type="groupname" value="calib2"/>
    <declrequirement name="calib_g3" type="groupname" value="calib3"/>

    <declrequirement name="online_g" type="groupname" value="online"/>

    <declrequirement name="odc_ncores_calib_its" type="custom" value="10"/>
    <declrequirement name="odc_ncores_calib_mft" type="custom" value="10"/>
    <declrequirement name="odc_ncores_calib_tof" type="custom" value="20"/>

    <declrequirement name="odc_zone_calib" type="custom" value="calib"/>
    <declrequirement name="odc_zone_reco" type="custom" value="online"/>
    ...
    <declcollection name="RecoCollection">
        <requirements>
            ...
            <name>online_g</name>
            <name>odc_zone_reco</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_its">
        <requirements>
            <name>odc_ncores_calib_its</name>
            <name>calib_g1</name>
            <name>odc_zone_calib</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_mft">
        <requirements>
            <name>odc_ncores_calib_mft</name>
            <name>calib_g2</name>
            <name>odc_zone_calib</name>
        </requirements>
        <tasks>
            ...
        </tasks>
    </declcollection>
    <declcollection name="calib_tof">
        <requirements>
            <name>odc_ncores_calib_tof</name>
            <name>calib_g3</name>
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

So, if you wished to create a topo with core-scheduling without the odc-epn-topo, you need to add custom requirements with `odc_ncores_...` and `odc_zone_...`.


### Terminology (to be extended)

As the AliECS/EPN/ODC/DDS terminology is quite large and even partially conflicting, we find it useful to summarize some of it here.

| Term | Description |
| ---- | ---- |
| ECS Environment | part of a whole experiment that can work independently on a task |
| ECS Partition | Same as ECS Environment, but with a human-readable label. Typically corresponds to one active DDS topology |
| Slurm Partition | Groups nodes managed by Slurm into logical sets |
| Run | A period of continuous data taking. Single ECS partition can contain multiple runs. Individual runs are separated by Running->Ready->Running state cycle |
| Zone | A label for a part of a resource request from AliECS. Maps to a Slurm partition, or a set of nodes managed through SSH or another resource manager |
| DDS Topology | A set of DDS tasks, potentially grouped into DDS Collections and/or multiplied via DDS Groups |
| DDS Task | A user executable or a script, launched through DDS |
| DDS Collection | A set of DDS Tasks, grouped together that has to be executed on the same node |
| DDS Group | A set of DDS Tasks and/or DDS Collections, grouped together only to allow easy way of multiplying them |
