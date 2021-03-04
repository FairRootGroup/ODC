# Resource Plugins

Resource plugin is an executable or script which generates RMS configuration for [`dds-submit`](http://dds.gsi.de/doc/nightly/dds-submit.html). It can be written in any programming language and doesn’t depend on ODC.

The contract between resource plugin and ODC:
 * **Input:** a resource description string. For example, it can be in JSON ar XML format or any other format. ODC executes plugin and provides a resource string via `--res` command line option:
```
myplugin --res "Resource description string"
```
 * **Output:** an RMS configuration in `stdout` in the [XML fomrat](rp.md#resource-description-format). 

### Resource description format
ODC uses XML with the following top level tags:
|Tag|Description|
|-------|-------|
|`<rms>`|Name of the DDS RMS plugin|
|`<configFile>`|Path to the configuration file|
|`<agents>`|Number of agents|
|`<slots>`|Number of slots per agent|
|`<requiredSlots>`|Required number of total active slots|
 
Each tag (except `<requiredSlots>`) corresponds to the command line option used by [`dds-submit`](http://dds.gsi.de/doc/nightly/dds-submit.html). Not all tags are required. A set of required tags depends on the used [DDS RMS plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html).

[`SubmitRequest`](grpc-proto/odc.proto) (as well as [`dds-submit`](http://dds.gsi.de/doc/nightly/dds-submit.html)) is async meaning that it doesn't wait for DDS agents to appear online. In order to make it sync one can optionally define the number of required active slots using `<requiredSlots>` tag.

### Built-in plugins

ODC provides the `odc-rp-same` plugin out of the box. The plugin prints to `stdout` a received resource description string. It can be used, for example, if ODC client (or ECS) generates the RMS configuration itself.

### Register custom plugins

In order to use custom resource plugin one need to register it in ODC server. `odc-grpc-server` and `odc-cli-server` have `--rp` command line option which allows to register custom plugins. For example:
```
odc-grpc-server --rp my_plugin:/path/to/my_plugin another_plugin:/path/to/another_plugin
```
Registered plugins can be addressed by using `plugin` field of [`SubmitRequest`](grpc-proto/odc.proto) or [`RunRequest`](grpc-proto/odc.proto).
