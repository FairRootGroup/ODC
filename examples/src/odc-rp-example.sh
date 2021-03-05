#!/usr/bin/env bash

# The plugin gets --res option as a first argument followed by a string.
# odc-rp-example --res "Resource description"

opt=$1
desc=$2

# Do some basic consistency check:
# * number of arguments is equal to 2
# * first argument is "--res"
if [[ "$#" -ne 2 || ${opt} != "--res" ]]; then
    echo "Illegal arguments. Usage: odc-rp-example --res \"Resource description\"."
    exit 1
fi

# Generate a sample DDS SSH config file
# In this sample example we assume that resource description containes either "config1" or "config2".
# Based on this string we create a static DDS SSH configuration.
# In a real case a resource description can be rather complicated, for example, in JSON or XML format.
sshConfigSample=""
requiredSlots=0
if [[ ${desc} = "config1" ]]; then
    sshConfigSample="
    @bash_begin@
    @bash_end@
    wn_sample_1, localhost, , ~/tmp/wn_dds, 10
    wn_sample_2, node.gsi.de, , ~/tmp/wn_dds, 10
    "
    requiredSlots=20
elif [[ ${desc} = "config2" ]]; then
    sshConfigSample="
    @bash_begin@
    source MyScript.sh
    @bash_end@
    wn_data_1, localhost, , ~/tmp/wn_dds, 20
    wn_data_2, node101.gsi.de, , ~/tmp/wn_dds, 20
    wn_data_3, node202.gsi.de, , ~/tmp/wn_dds, 20
    "
    requiredSlots=60
else
    echo "Illegal resource description. Use \"config1\" or \"config2\""
    exit 1
fi

# Save a sample DDS SSH config file
sshConfigFile="my_dds_ssh_config.cfg"
echo "${sshConfigSample}" > ${sshConfigFile}

# For SSH configuration the required fields are <rms>, <configFile> and <requiredSlots>
# Print output to stdout
echo "<rms>ssh</rms>
<configFile>${sshConfigFile}</configFile>
<requiredSlots>${requiredSlots}</requiredSlots>"

exit 0
