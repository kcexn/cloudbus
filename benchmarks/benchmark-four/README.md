# Cloudbus Benchmark: Multi-server Half-Duplex with Latching
This a benchmark using 1-to-N half-duplex with reply path latching and cloudbus as the 
gateway.

## Results:
Latencies (ms): mean=2, median=2, p95=2, p99=3

## Benchmarking on Google Compute Engine with Gcloud CLI:
### Building the Test Infrastructure
First you may need to authenticate the gcloud CLI by running `gcloud auth login` and following 
the prompts. Then modify the variables in `.gce-environment` to define the configuration for 
the test infrastructure before sourcing it:
```
$ cat .gce-environment
deactivate >/dev/null 2>&1
deactivate() {
    unset SERVER3_DISK
    unset SERVER3_ZONE
    unset SERVER3_NAME
    unset SERVER2_DISK
    unset SERVER2_ZONE
    unset SERVER2_NAME
    unset SERVER1_DISK
    unset SERVER1_ZONE
    unset SERVER1_NAME
    unset GATEWAY_ZONE
    unset GATEWAY_DISK
    unset GATEWAY_NAME
    unset CLIENT_ZONE
    unset CLIENT_DISK
    unset CLIENT_NAME
    unset DISK_TYPE
    unset MACHINE_TYPE
    unset deactivate
}
export MACHINE_TYPE='e2-micro'
DISK_TYPE='auto-delete=yes,boot=yes,image=projects/debian-cloud/global/images/debian-12-bookworm-v20250415,mode=rw,size=10,type=pd-balanced'
export CLIENT_NAME='client'
export CLIENT_DISK="${DISK_TYPE},device-name=${CLIENT_NAME}"
export CLIENT_ZONE='australia-southeast2-a'
export GATEWAY_NAME='gateway'
export GATEWAY_DISK="${DISK_TYPE},device-name=${GATEWAY_NAME}"
export GATEWAY_ZONE='australia-southeast2-a'
export SERVER1_NAME='server1'
export SERVER1_ZONE='australia-southeast2-a'
export SERVER1_DISK="${DISK_TYPE},device-name=${SERVER1_NAME}"
export SERVER2_NAME='server2'
export SERVER2_ZONE='australia-southeast2-b'
export SERVER2_DISK="${DISK_TYPE},device-name=${SERVER2_NAME}"
export SERVER3_NAME='server3'
export SERVER3_ZONE='australia-southeast2-c'
export SERVER3_DISK="${DISK_TYPE},device-name=${SERVER3_NAME}"
$ . .gce-environment
```
Deploy the test infrastructure by running:
```
$ gcloud compute instances create "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --create-disk="${CLIENT_DISK}" \
    --machine-type="${MACHINE_TYPE}" && \
gcloud compute instances create "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --create-disk="${GATEWAY_DISK}" \
    --machine-type="${MACHINE_TYPE}" && \
gcloud compute instances create "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --create-disk="${SERVER1_DISK}" \
    --machine-type="${MACHINE_TYPE}" && \
gcloud compute instances create "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --create-disk="${SERVER2_DISK}" \
    --machine-type="${MACHINE_TYPE}" && \
gcloud compute instances create "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --create-disk="${SERVER3_DISK}" \
    --machine-type="${MACHINE_TYPE}"
```

### Installing Pre-requisite Software:
#### Installing the HTTP servers.:
We will use NGINX and we install it by running:
```
$ gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install nginx'" && \
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install nginx'" && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install nginx'"
```
and following the prompts.

#### Installing the bus:
Install the compiler and dependencies:
```
$ gcloud compute ssh "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install build-essential libpcre2-dev libc-ares-dev'" && \
gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install build-essential libpcre2-dev libc-ares-dev'" && \
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install build-essential libpcre2-dev libc-ares-dev'" && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y upgrade && \
        sudo apt-get -y install build-essential libpcre2-dev libc-ares-dev'"
```

Copy the cloudbus distribution:
```
$ VERSION=0.4.0 && \
CLOUDBUS_PATH="./cloudbus-${VERSION}.tar.gz" && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${GATEWAY_NAME}":~ \
    --zone="${GATEWAY_ZONE}" && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${SERVER1_NAME}":~ \
    --zone="${SERVER1_ZONE}" && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${SERVER2_NAME}":~ \
    --zone="${SERVER2_ZONE}" && \
gcloud compute scp "${CLOUDBUS_PATH}" \
    "${SERVER3_NAME}":~ \
    --zone="${SERVER3_ZONE}"
```

Install cloudbus:
```
$ VERSION=0.4.0 && \
COMMAND="/usr/bin/sh -c 'tar -zxvf cloudbus-${VERSION}.tar.gz && \
    cd cloudbus-${VERSION} && \
    ./configure CXXFLAGS=-flto && \
    make -j2 && \
    sudo make install' " && \
gcloud compute ssh "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --command="${COMMAND}" && \
gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="${COMMAND}" && \
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="${COMMAND}" && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="${COMMAND}"
```

Configure cloudbus:
```
$ cat conf/controller.ini
[Service0]
bind=tcp://0.0.0.0:8080
backend=tcp://<SERVER1_NAME>.<SERVER1_ZONE>:8080
backend=tcp://<SERVER2_NAME>.<SERVER2_ZONE>:8080
backend=tcp://<SERVER3_NAME>.<SERVER3_ZONE>:8080
$ cat conf/server.ini
[Service0]
bind=tcp://0.0.0.0:8080
backend=tcp://localhost:80
$ gcloud compute scp conf/controller.ini "${GATEWAY_NAME}":~ \
    --zone="${GATEWAY_ZONE}" && \
gcloud compute scp conf/segment.ini "${SERVER1_NAME}":~ \
    --zone="${SERVER1_ZONE}" && \
gcloud compute scp conf/segment.ini "${SERVER2_NAME}":~ \
    --zone="${SERVER2_ZONE}" && \
gcloud compute scp conf/segment.ini "${SERVER3_NAME}":~ \
    --zone="${SERVER3_ZONE}" && \
gcloud compute ssh "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv controller.ini /usr/local/etc/cloudbus/' " && \
gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.ini /usr/local/etc/cloudbus/' " && \
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.ini /usr/local/etc/cloudbus/' " && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.ini /usr/local/etc/cloudbus/' "
```

Copy the controller and segment service configs:
```
$ PREFIX='/usr/local/etc/cloudbus/systemd' && \
SYSTEM_PREFIX='/etc/systemd/system' && \
gcloud compute scp conf/controller.service "${GATEWAY_NAME}":~ \
    --zone="${GATEWAY_ZONE}" && \
gcloud compute scp conf/segment.service "${SERVER1_NAME}":~ \
    --zone="${SERVER1_ZONE}" && \
gcloud compute scp conf/segment.service "${SERVER2_NAME}":~ \
    --zone="${SERVER2_ZONE}" && \
gcloud compute scp conf/segment.service "${SERVER3_NAME}":~ \
    --zone="${SERVER3_ZONE}" && \
gcloud compute ssh "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv controller.service ${PREFIX}/' " && \
gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.service ${PREFIX}/' " && \
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.service ${PREFIX}/' " && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="/usr/bin/sh -c 'sudo mv segment.service ${PREFIX}/' "
```

Start the service:
```
$ gcloud compute ssh "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --command="/usr/bin/sh -c 'sudo ln -s ${PREFIX}/controller.service ${SYSTEM_PREFIX}/controller.service && \
        sudo systemctl enable controller && \
        sudo systemctl start controller' " && \
gcloud compute ssh "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --command="/usr/bin/sh -c 'sudo ln -s ${PREFIX}/segment.service ${SYSTEM_PREFIX}/segment.service && \
        sudo systemctl enable segment && \
        sudo systemctl start segment' "
gcloud compute ssh "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --command="/usr/bin/sh -c 'sudo ln -s ${PREFIX}/segment.service ${SYSTEM_PREFIX}/segment.service && \
        sudo systemctl enable segment && \
        sudo systemctl start segment' " && \
gcloud compute ssh "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --command="/usr/bin/sh -c 'sudo ln -s ${PREFIX}/segment.service ${SYSTEM_PREFIX}/segment.service && \
        sudo systemctl enable segment && \
        sudo systemctl start segment' "
```

#### Install a Load Tester on the Test Client:
We will use Apache Jmeter. We can install it by running:
```
$ gcloud compute ssh "${CLIENT_NAME}" \
    --zone="${CLIENT_ZONE}" \
    --command="/usr/bin/sh -c 'sudo apt-get update && \
        sudo apt-get -y install default-jre && \
        (wget https://dlcdn.apache.org//jmeter/binaries/apache-jmeter-5.6.3.tgz -O - | sudo tar -zxvf - -C /opt/) && \
        sudo ln -s /opt/apache-jmeter-5.6.3/bin/jmeter /usr/bin/jmeter' "
```
and following the prompts.

### Configure the HTTP Tests:
Copy the Jmeter JMX benchmark to the test client by running:
```
$ BENCHMARK_PATH='./Single Server Benchmark.jmx' && \
gcloud compute scp "${BENCHMARK_PATH}" \
    "${CLIENT_NAME}":~ \
    --zone="${CLIENT_ZONE}"
```

### Execute the HTTP Tests:
Then execute the tests by running:
```
$ COMMAND="/usr/bin/sh -c 'rm -f results.csv && \
    jmeter -n -t Single\ Server\ Benchmark.jmx \
        -Jof=./results.csv \
        -Jhost=${GATEWAY_NAME}.${GATEWAY_ZONE} \
        -Jport=8080 \
        -Jthreads=64 \
        -Jrequests=6000 \
        -Jduration=240'" && \
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
gcloud compute instances delete "${GATEWAY_NAME}" \
    --zone="${GATEWAY_ZONE}" \
    --quiet && \
gcloud compute instances delete "${SERVER1_NAME}" \
    --zone="${SERVER1_ZONE}" \
    --quiet && \
gcloud compute instances delete "${SERVER2_NAME}" \
    --zone="${SERVER2_ZONE}" \
    --quiet && \
gcloud compute instances delete "${SERVER3_NAME}" \
    --zone="${SERVER3_ZONE}" \
    --quiet
```
Optionally, you may remove the environment variables set by `.gce-environment` with:
```
$ deactivate
```
And finally, you may logout of your gcloud session with `gcloud auth revoke <ACCOUNT>`.
