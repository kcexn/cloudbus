# Reference Benchmark: Single Web Server
This benchmark measures the performance of a single web-server and is the control benchmark 
by which other benchmarks can be compared. This benchmark specifies the common build 
configuration of similar single web-server benchmarks. These build specifications will be 
provided in collections of environment files that can be configured for a specific network environment. 
Currently, I have only defined `.gce-environment` to be used for building and configuring servers on Google 
Compute Engine (GCE) because that is what I am most familiar with. Other network environments 
like AWS EC2, private clouds (e.g., OpenStack), or traditional networks are not currently 
supported.

## Results:
Latencies (ms): mean=29, median=29, p95=30, p99=32

## Benchmarking on Google Compute Engine with Gcloud CLI:
### Building the Test Infrastructure
First you may need to authenticate the gcloud CLI by running `gcloud auth login` and following 
the prompts. Then modify the variables in `.gce-environment` to define the configuration for 
the test infrastructure before sourcing it:
```
$ cat .gce-environment
deactivate >/dev/null 2>&1
deactivate() {
    unset SERVER_DISK \
        SERVER_ZONE \
        SERVER_NAME \
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
export SERVER_NAME='test-server'
export SERVER_ZONE='australia-southeast2-c'
export SERVER_DISK="${DISK_TYPE},device-name=${SERVER_NAME}"
$ . .gce-environment
```
Deploy the test infrastructure by running:
```
$ gcloud compute instances create ${CLIENT_NAME} \
    --zone=${CLIENT_ZONE} \
    --machine-type=${MACHINE_TYPE} \
    --create-disk=${CLIENT_DISK} && \
gcloud compute instances create ${SERVER_NAME} \
    --zone=${SERVER_ZONE} \
    --machine-type=${MACHINE_TYPE} \
    --create-disk=${SERVER_DISK}
```

### Installing Pre-requisite Software:
#### Install an HTTP server on the Test Server:
We will use NGINX and we install it by running:
```
$ gcloud compute ssh ${SERVER_NAME} \
    --zone=${SERVER_ZONE} \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get upgrade && \
        sudo apt-get install nginx'"
```
and following the prompts.

#### Install a Load Tester on the Test Client:
We will use Apache Jmeter. We can install it by running:
```
$ gcloud compute ssh ${CLIENT_NAME} \
    --zone=${CLIENT_ZONE} \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get upgrade && \
        sudo apt-get install default-jre && \
        (wget https://dlcdn.apache.org//jmeter/binaries/apache-jmeter-5.6.3.tgz -O - | sudo tar -zxvf - -C /opt/) && \
        sudo ln -s /opt/apache-jmeter-5.6.3/bin/jmeter /usr/bin/jmeter'"
```
and following the prompts.

### Configure the HTTP Tests:
Copy the Jmeter JMX benchmark to the test client by running:
```
$ BENCHMARK_PATH='./Single Server Benchmark.jmx' && \
gcloud compute scp "${BENCHMARK_PATH}" \
    ${CLIENT_NAME}:~ \
    --zone=${CLIENT_ZONE}
```

### Execute the HTTP Tests:
Execute the tests by running:
```
$ COMMAND="/usr/bin/sh -c 'rm -f results.csv && \
    jmeter -n -t Single\ Server\ Benchmark.jmx \
        -Jof=./results.csv \
        -Jhost=${SERVER_NAME}.${SERVER_ZONE} \
        -Jport=80 \
        -Jthreads=256 \
        -Jrequests=6000 \
        -Jduration=240'" && \
gcloud compute ssh "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="${COMMAND}"
```
Then retrieve the test artifacts by running:
```
$ ARTIFACTS_PATH='./artifacts/' && \
gcloud compute scp ${CLIENT_NAME}:~/results.csv \
    "${ARTIFACTS_PATH}" \
    --zone=${CLIENT_ZONE}
```

### Tearing Down the Test Infrastructure
Delete the compute instances by running:
```
$ gcloud compute instances delete ${CLIENT_NAME} \
    --zone=${CLIENT_ZONE} \
    --quiet && \
gcloud compute instances delete ${SERVER_NAME} \
    --zone=${SERVER_ZONE} \
    --quiet
```
Optionally, you may remove the environment variables set by `.gce-environment` with:
```
$ deactivate
```
And finally, you may logout of your gcloud session with `gcloud auth revoke <ACCOUNT>`.
