# Resource Plugins

Resource plugin is an executable or script which generates RMS configuration for [`dds-submit`](http://dds.gsi.de/doc/nightly/dds-submit.html). It can be written in any programming language and doesn’t depend on ODC.

The contract between resource plugin and ODC:
 * **Input:** a resource description string. For example, it can be in JSON ar XML format or any other format. ODC executes plugin and provides a resource string via `--res` and partition ID via `--id` command line options:
```
myplugin --res "Resource description string" --id "Partition ID"
```
 * **Output:** an RMS configuration in `stdout` in the [XML format](rp.md#resource-description-format). 

### Resource description format
ODC uses XML with the following top level tags:
|Tag|Description|
|-------|-------|
|`<rms>`|Name of the DDS RMS plugin|
|`<configFile>`|Path to the configuration file|
|`<agents>`|Number of agents|
|`<slots>`|Number of slots per agent|

Each tag corresponds to the command line option used by [`dds-submit`](http://dds.gsi.de/doc/nightly/dds-submit.html). Not all tags are required. A set of required tags depends on the used [DDS RMS plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html).

### Built-in plugins

ODC provides the `odc-rp-same` plugin out of the box. The plugin prints to `stdout` a received resource description string. It can be used, for example, if ODC client (or ECS) generates the RMS configuration itself.

ODC also implemens `odc-rp-epn` plugin for `EPN` project of `ALICE`. The plugin queries node allocation info via a special `epnc` service using `gRPC` and generates SSH configuration file for DDS. Plugin expects JSON resource description provided via `--res` option. Either a single resource or an array of resources can be requested:
```JSON
{
   "zone":"online",
   "n":10
}
```
or
```JSON
[{
   "zone":"online",
   "n":10
 },
 {
   "zone":"calib",
   "n":1
}]
```
`zone` is the zone name. `n` is the number of EPN nodes.

### Register custom plugins

In order to use custom resource plugin one need to register it in ODC server. `odc-grpc-server` and `odc-cli-server` have `--rp` command line option which allows to register custom plugins. For example:
```bash
odc-grpc-server --rp my_plugin:/path/to/my_plugin another_plugin:/path/to/another_plugin
```
Registered plugins can be addressed by using `plugin` field of [`SubmitRequest`](grpc-proto/odc.proto) or [`RunRequest`](grpc-proto/odc.proto).

### Resource plugin examples

ODC containes several examples of resource plugins:
 * A built-in [`odc-rp-same`](../plugins/rp-same/odc-rp-same.cpp) plugin implemented in `C++`.
 * A built-in [`odc-rp-epn`](../plugins/rp-epn/) plugin uses `gRPC` and implemented in `C++`.
 * A built-in [`odc-rp-epn-slurm`](../plugins/rp-epn-slurm/) Receives resources description from AliECS via gRPC and translates these into DDS slurm configuration.
 * A [DDS SSH configuration plugin](../examples/odc-rp-example.sh) implemented as a `bash` script.
