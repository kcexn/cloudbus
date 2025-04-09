# Cloudbus Benchmark: Single Web Server
This benchmark measures the performance of the cloudbus components when bridging traffic across different regions. 
As it is a single web-server configuration, the results may be compared against the results in benchmark-one. 
Compared to the configuration in benchmark-one, Cloudbus adds to additional network hops, one from the client to the controller, 
then one more from the controller to the segment. Despite this, there is no signiciant measurable penalty to the use of Cloudbus in 
GCE.

## Results:
Latencies (ms): mean=30, median=30, p99=32

## Benchmarking on Google Compute Engine with Gcloud CLI:
### Building the Test Infrastructure
First you may need to authenticate the gcloud CLI by running `gcloud auth login` and following 
the prompts. Then modify the variables in `.gce-environment` to define the configuration for 
the test infrastructure before sourcing it:
```
$ cat .gce-environment
deactivate >/dev/null 2>&1
deactivate() {
    unset SEGMENT_DISK \
        SEGMENT_NAME \
        SERVER_DISK \
        SERVER_ZONE \
        SERVER_NAME \
        CONTROLLER_DISK \
        CONTROLLER_NAME \
        CLIENT_DISK \
        CLIENT_ZONE \
        CLIENT_NAME \
        DISK_TYPE \
        MACHINE_TYPE \
        deactivate
}
export MACHINE_TYPE='e2-micro'
DISK_TYPE='auto-delete=yes,boot=yes,image=projects/debian-cloud/global/images/debian-12-bookworm-v20250311,mode=rw,size=10,type=pd-balanced'
export CLIENT_NAME='test-client'
export CLIENT_ZONE='australia-southeast1-c'
export CLIENT_DISK="${DISK_TYPE},device-name=${CLIENT_NAME}"
export CONTROLLER_NAME='test-controller'
export CONTROLLER_DISK="${DISK_TYPE},device-name=${CONTROLLER_NAME}"
export SERVER_NAME='test-server'
export SERVER_ZONE='australia-southeast2-c'
export SERVER_DISK="${DISK_TYPE},device-name=${SERVER_NAME}"
export SEGMENT_NAME='test-segment'
export SEGMENT_DISK="${DISK_TYPE},device-name=${SEGMENT_NAME}"
$ . .gce-environment
```
Deploy the test infrastructure by running:
```
$ gcloud compute instances create "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --machine-type="${MACHINE_TYPE}" \
    --create-disk="${CLIENT_DISK}" && \
gcloud compute instances create "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --machine-type="${MACHINE_TYPE}" \
    --create-disk="${CONTROLLER_DISK}" && \    
gcloud compute instances create "${SERVER_NAME}" \
    --zone="${SERVER_ZONE}" \
    --machine-type="${MACHINE_TYPE}" \
    --create-disk="${SERVER_DISK}" && \
gcloud compute instances create "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --machine-type="${MACHINE_TYPE}" \
    --create-disk="${SEGMENT_DISK}"
```

### Installing Pre-requisite Software:
#### Install an HTTP server on the Test Server:
We will use NGINX and we install it by running:
```
$ gcloud compute ssh "${SERVER_NAME}" \
    --zone="${SERVER_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && sudo apt-get install nginx'"
```
and following the prompts.

#### Install a Load Tester on the Test Client:
We will use Apache Jmeter. We can install it by running:
```
$ gcloud compute ssh "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
    sudo apt-get install default-jre && \
    (wget https://dlcdn.apache.org//jmeter/binaries/apache-jmeter-5.6.3.tgz -O - | sudo tar -zxvf - -C /opt/) && \
    sudo ln -s /opt/apache-jmeter-5.6.3/bin/jmeter /usr/bin/jmeter'"
```
and following the prompts.

#### Install the c++ compiler on the Controller and Segment:
Install build tools and cloudbus dependencies on the controller and segment nodes by running:
```
$ gcloud compute ssh "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
    sudo apt-get install build-essential libc-ares-dev'" && \
gcloud compute ssh "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
    sudo apt-get install build-essential libc-ares-dev'"
```

#### Install and Configure Cloudbus on the Controller and the Segment
Copy the cloudbus distribution:
```
$ CLOUDBUS_PATH='./cloudbus-0.0.2.tar.gz' && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${CONTROLLER_NAME}":~ \
    --zone="${CLIENT_ZONE}" && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${SEGMENT_NAME}":~ \
    --zone="${SERVER_ZONE}"
```
Install cloudbus:
```
$ COMMAND="/usr/bin/sh -c 'tar -zxvf cloudbus-0.0.2.tar.gz && \
    cd cloudbus-0.0.2 && \
    ./configure && \
    make && \
    sudo make install'" && \
gcloud compute ssh "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="${COMMAND}" && \
gcloud compute ssh "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --command="${COMMAND}"
```

Configure cloudbus:
```
$ cat conf/controller.ini
[Cloudbus]

[Service0]
bind=tcp://0.0.0.0:8080
backend=tcp://<SEGMENT_NAME>.<SERVER_ZONE>:8080
$ cat conf/segment.ini
[Cloudbus]

[Service0]
bind=tcp://0.0.0.0:8080
backend=tcp://<SERVER_NAME>.<SERVER_ZONE>:80
$ gcloud compute scp conf/controller.ini "${CONTROLLER_NAME}":~ \
    --zone="${CLIENT_ZONE}" && \
gcloud compute scp conf/segment.ini "${SEGMENT_NAME}":~ \
    --zone="${SERVER_ZONE}"
$ gcloud compute ssh "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv controller.ini /usr/local/etc/cloudbus/'" && \
gcloud compute ssh "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.ini /usr/local/etc/cloudbus/'"
```

### Configure the HTTP Tests:
Copy the Jmeter JMX benchmark to the test client by running:
```
$ BENCHMARK_PATH='./Single Server Benchmark.jmx' && \
gcloud compute scp "${BENCHMARK_PATH}" \
    "${CLIENT_NAME}":~ \
    --zone="${CLIENT_ZONE}"
```

### Execute the HTTP Tests:
In separate shells, start the controller and segment binaries:
```
$ gcloud compute ssh "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="/usr/bin/sh -c controller"
$ gcloud compute ssh "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --command="/usr/bin/sh -c segment"
```
Then execute the tests by running:
```
$ COMMAND="/usr/bin/sh -c 'jmeter -n -t Single\ Server\ Benchmark.jmx \
    -Jof=./results.csv \
    -Jhost="${CONTROLLER_NAME}.${CLIENT_ZONE}" \
    -Jport=8080 \
    -Jrequests=1000 \
    -Jduration=30'" && \
gcloud compute ssh "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="${COMMAND}"
```
Then retrieve the test artifacts by running:
```
$ ARTIFACTS_PATH='./artifacts/' && \
gcloud compute scp "${CLIENT_NAME}":results.csv \
    "${ARTIFACTS_PATH}" \
    --zone="${CLIENT_ZONE}"
```

### Tearing Down the Test Infrastructure
Delete the compute instances by running:
```
$ gcloud compute instances delete "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --quiet && \
gcloud compute instances delete "${CONTROLLER_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --quiet && \
gcloud compute instances delete "${SERVER_NAME}" \
    --zone="${SERVER_ZONE}" \
    --quiet && \
gcloud compute instances delete "${SEGMENT_NAME}" \
    --zone="${SERVER_ZONE}" \
    --quiet
```
Optionally, you may remove the environment variables set by `.gce-environment` with:
```
$ deactivate
```
And finally, you may logout of your gcloud session with `gcloud auth revoke <ACCOUNT>`.
