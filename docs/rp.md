# Resource Plugins

Resource plugin is an executable or script which generates RMS configuration for `dds-submit`. It can be written in any programming language and doesn’t depend on ODC.

The contract between resource plugin and ODC:
 * **Input:** a resource description string. For example, it can be in JSON ar XML format or any other format. ODC executes plugin and provides a resource string via `--res` command line option:
```
myplugin --res "Resource description string"
```
 * **Output:** an RMS configuration in `stdout` in the [XML fomrat](rp.md#resource-description-format). 

### Built-in plugins

ODC provides the `odc-rp-same` plugin out of the box. The plugin prints to `stdout` a received resource description string. It can be used, for example, if ODC client (or ECS) generates the RMS configuration itself.

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

`Submit` request (as well as `dds-submit`) is async meaning that it doesn't wait for agents to appear online. In order to make it sync one can optionally define the number of required active slots using  `<requiredSlots>` tag.
