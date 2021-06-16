# Frequently Asked Questions

### Does ODC support multiple active topologies?

Yes. ODC is able to manage multiple DDS sessions concurrently. In this case, each DDS session is an independent active topology. But [DDS](http://dds.gsi.de) itself can have only one active topology at a time. Moreover, a number of features in ODC/[DDS](https://github.com/FairRootGroup/DDS)/[FairMQ](https://github.com/FairRootGroup/FairMQ) allow to support a different use case scenarios. 

  * DDS topology API allows to create a topology either by editing an XML file or programmatically using C++ or even combining these two approaches. For instance, we have a DDS topology exported from DPL and another topology describing Data Distribution. We can programmatically combine these two topologies into a single topology which can be activated by ODC. [Here](../examples/src/odc-topo.cpp) is an example of how to do it in a couple of lines of code.

  * [FairMQ SDK](https://github.com/FairRootGroup/FairMQ/tree/master/fairmq/sdk), as well as ODC, allow to address only a part of the topology when changing the states. This is done by providing a regular expression of the task paths which states has to be changed. For instance, you can change only the states of `TfBuilder` task.


### What is a path in DDS topology?

Each task in a DDS topology can be referenced by a path. A topology has a hierarchical structure which is reflected in the path. It is similar to a filesystem path. There are two types of topology paths. First type is used to identify declared topology elements. Second type is used during a runtime. For instance, "main/group1/collection1/task1" is a path for a `task1` task declaration. And `main/group1/collection1_4/task1_0` is a runtime path of one of the `task1` tasks. Note `_X`. It's an index of a particular task or collection which is assigned to task during the runtime. Each collection and task has a unique path during a runtime.

Having a topology like this:
```xml
<topology name="topo1">
    <decltask name="task1">
        <exe>task1.exe</exe>
    </decltask>
    <decltask name="task2">
        <exe>task2.exe</exe>
    </decltask>
    <declcollection name="collection1">
       <tasks>
           <name>task1</name>
           <name n="2">task2</name>
       </tasks>
    </declcollection>
    <main name="main">
       <task>task1</task>
       <collection>collection1</collection>
       <group name="group1" n="2">
          <task>task2</task>
          <collection>collection1</collection>
       </group>
    </main>
</topology>
```
Results in the following paths of declared tasks:
```
main/task1
main/collection1/task1
main/collection1/task2
main/group1/task2
main/group1/collection1/task1
main/group1/collection1/task2
```
For runtime the list of paths is the following:
```
main/task1_0
main/collection1_0/task1_0
main/collection1_0/task2_0
main/collection1_0/task2_1
main/group1/task2_0
main/group1/collection1_0/task1_0
main/group1/collection1_0/task2_0
main/group1/collection1_0/task2_1
main/group1/task2_1
main/group1/collection1_1/task1_0
main/group1/collection1_1/task2_0
main/group1/collection1_1/task2_1
```
Empty path refers to a broadcast to all tasks in the topology. If you want to address a particular set of tasks in the topology you can use a regular expression.

Also each task and collection has a hash (`uint64_t`) which can be used to address a task or a collection.

Paths as well as other runtime information of task and collection is populated as environment variables and available for each task in the topology.

See more on DDS topology [here](http://dds.gsi.de/doc/nightly/topology.html).
