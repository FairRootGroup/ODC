# ODC Release Notes

## v0.6 (NOT YET RELEASED)

### ODC common
Added: new Run request which combines Initialize, Submit and Activate. Run request always creates a new DDS session.    
Added: new GetState request which returns a current aggregated state of FairMQ devices.    

### ODC gRPC protocol
Modified: documentation of the proto file.    
Modified: StateChangeRequest changed to StateRequest.    
Modified: StateChangeReply changed to StateReply. New `state` field containing aggregated device state was added.    
Modified: implement SetPropertiesRequest instead of SetPropertyRequest. Multiple properties can be set with a single request.     


## v0.4 (2020-07-01)

### ODC common
Added: possibility to attach to the running DDS session.    
Added: DDS session ID in each reply.    
Added: set request timeout via command line interface.    
Added: unit tests.    
Modified: minimum required DDS version is 3.4    


## v0.2 (2020-05-13)

The first stable internal release.
