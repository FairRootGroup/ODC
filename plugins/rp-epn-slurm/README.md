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

### Terminology (to be extended)

As the AliECS/EPN/ODC/DDS terminology is quite large and even partially conflicting, we find it useful to summarize some of it here.

| Term | Description |
| ---- | ---- |
| ECS Environment | In Alice's definition it is "part of a whole experiment that can work independently on a task" |
| ECS Partition | Same as ECS Environment, but can be a human-readable label. Typically ECS Environment/Partition corresponds to one active DDS topology |
| Slurm Partition | Groups nodes managed by Slurm into logical sets |
| Run | Run in an ECS partition - a period of continuous data taking. Single ECS partition can contain multiple runs. Individual runs would then be separated by Running->Ready->Running state changes |
| Zone | A label for a part of a resource request from AliECS. Typicaly maps to a Slurm partition, or a set of nodes managed through SSH or another resource manager |
| DDS Topology | A set of DDS tasks, potentially grouped into DDS Collections and/or multiplied via DDS Groups |
| DDS Task | A user executable or a script, launched through DDS |
| DDS Collection | A set of DDS Tasks, grouped together that has to be executed on the same node |
| DDS Group | A set of DDS Tasks and/or DDS Collections, grouped together only to allow easy way of multiplying them |
