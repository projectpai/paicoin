# How to develop and test with Kubernetes
This solution enables us to run HybridConsensus in a Kubernetes cluster. These instructions are for Linux, but should be similar on other OS.

## Install prerequisites:

Install Minikube, see instructions here: https://kubernetes.io/docs/tasks/tools/install-minikube/

## Start the Minikube cluster

Start the local Kubernetes cluster:
```
$ minikube start
```

After starting status should look like this:
```
$ minikube status
minikube
type: Control Plane
host: Running
kubelet: Running
apiserver: Running
kubeconfig: Configured
```

## Build the required images

We need 3 docker images to be able to run the kubernetes deployment.
 1. the hybrid-consensus paicoind
 2. the cpuminer
 3. the block-explorer. 
 
We will build paicoind and cpuminer images on the development machine (not inside Minikube) for performance reasons.
At the moment the block-explorer only needs to be dowloaded from dreg-or.oben.me/projectpai/block-explorer:latest.

First, we'll build the paicoind image:
```
$ ./build.sh 0.16.1 feature/hybrid-consensus/kubernetes
```

The build script needs a version (recommended: use  MAJOR.minor.patch format to avoid packaging errors), and the name of the branch you wish to build (used repo is https://github.com/projectpai/paicoin)
This will build 2 images: one named "paicoin-build" that has all developement tools installed and is only needed to build a deb package to install paicoind in a second ubuntu image.
If this runs successfully you should have something similar to this:
```
$ docker images
REPOSITORY                                  TAG                 IMAGE ID            CREATED             SIZE
feature/hybrid-consensus/kubernetes         0.16.1              6644577b93cc        5 seconds ago       181MB
paicoin-build                               latest              1e7a9f358e42        10 days ago         771MB
```

To build the cpuminer image, first clone https://github.com/projectpai/cpuminer and cd into cpuminer folder.
```
$ docker build --rm -t cpuminer -f Dockerfile .
```
Note: make sure you point the "RUN git clone" command, in the Dockerfile, to the correct repo/branch.
```
$ docker images
REPOSITORY                                  TAG                 IMAGE ID            CREATED             SIZE
feature/hybrid-consensus/kubernetes         0.16.1              6644577b93cc        5 seconds ago       181MB
paicoin-build                               latest              1e7a9f358e42        10 days ago         771MB
cpuminer                                    latest              a3a56f1d10c7        5 minutes ago       347MB
```

Lastly pull the block-explorer image:
```
$ docker pull dreg-or.oben.me/projectpai/block-explorer
```

## Copy the images inside Minikube

In a fresh terminal, we will save the three images using the local installation of Docker:
```
$ docker save feature/hybrid-consensus/kubernetes:0.16.1 > img_paicoind 
$ docker save cpuminer > img_cpuminer 
$ docker save dreg-or.oben.me/projectpai/block-explorer > img_block_explorer 
```

We need the Docker internal to MiniKube. To point your shell to minikube's docker-daemon, run:
You need to run this command to be able to fetch images from the local disk later:
```
$ eval $(minikube -p minikube docker-env)
```

In Minikube, we don't yet see the images, but there are some k8s specific ones:
```
$ docker images
REPOSITORY                                  TAG                 IMAGE ID            CREATED             SIZE
kubernetesui/dashboard                      v2.0.0              8b32422733b3        2 months ago        222MB
k8s.gcr.io/kube-proxy                       v1.18.2             0d40868643c6        2 months ago        117MB
k8s.gcr.io/kube-apiserver                   v1.18.2             6ed75ad404bd        2 months ago        173MB
k8s.gcr.io/kube-controller-manager          v1.18.2             ace0a8c17ba9        2 months ago        162MB
k8s.gcr.io/kube-scheduler                   v1.18.2             a3099161e137        2 months ago        95.3MB
k8s.gcr.io/pause                            3.2                 80d28bedfe5d        4 months ago        683kB
k8s.gcr.io/coredns                          1.6.7               67da37a9a360        5 months ago        43.8MB
k8s.gcr.io/etcd                             3.4.3-0             303ce5db0e90        8 months ago        288MB
kubernetesui/metrics-scraper                v1.0.2              3b08661dc379        8 months ago        40.1MB
gcr.io/k8s-minikube/storage-provisioner     v1.8.1              4689081edb10        2 years ago         80.8MB
```

In the terminal with Minikube run these commands:
```
$ docker load < img_paicoind
$ docker load < img_cpuminer
$ docker load < img_block_explorer
```
We also need to pull redis, mongo and busybox official images:
```
$ docker pull mongo
$ docker pull redis
$ docker pull busybox:1.28
```

Check all images are loaded into the Minikube local Docker:
```
$ docker images
REPOSITORY                                  TAG                 IMAGE ID            CREATED             SIZE
mongo                                       latest              6d11486a97a7        6 days ago          388MB
feature/hybrid-consensus/kubernetes         0.16.1              669e744d243a        11 days ago         170MB
cpuminer                                    latest              457360275f6b        3 weeks ago         347MB
redis                                       latest              235592615444        4 weeks ago         104MB
kubernetesui/dashboard                      v2.0.0              8b32422733b3        2 months ago        222MB
k8s.gcr.io/kube-proxy                       v1.18.2             0d40868643c6        2 months ago        117MB
k8s.gcr.io/kube-apiserver                   v1.18.2             6ed75ad404bd        2 months ago        173MB
k8s.gcr.io/kube-controller-manager          v1.18.2             ace0a8c17ba9        2 months ago        162MB
k8s.gcr.io/kube-scheduler                   v1.18.2             a3099161e137        2 months ago        95.3MB
k8s.gcr.io/pause                            3.2                 80d28bedfe5d        4 months ago        683kB
k8s.gcr.io/coredns                          1.6.7               67da37a9a360        5 months ago        43.8MB
k8s.gcr.io/etcd                             3.4.3-0             303ce5db0e90        8 months ago        288MB
kubernetesui/metrics-scraper                v1.0.2              3b08661dc379        8 months ago        40.1MB
dreg-or.oben.me/projectpai/block-explorer   latest              91ed8a427f0f        12 months ago       190MB
busybox                                     1.28                8c811b4aec35        2 years ago         1.15MB
gcr.io/k8s-minikube/storage-provisioner     v1.8.1              4689081edb10        2 years ago         80.8MB
```

## Configure settings

Optionally, if you need different settings for paicoind, edit the "paicoin-config-map" in paicoind-deploy.yaml.

## Deploying the Minikube cluster

In a new terminal we deploy the paicoind nodes, the miner, the block-explorer, and the required services using a single command:
```
$ kubectl create -f paicoind-deploy.yaml
service/paicoin-service1 created
service/paicoin-service2 created
service/cpuminer-service created
service/explorer-service created
configmap/paicoin-config-map created
statefulset.apps/paicoin-node1 created
statefulset.apps/paicoin-node2 created
statefulset.apps/cpuminer created
statefulset.apps/explorer created

```

To monitor, use the following:
```
$ kubectl get all
NAME                  READY   STATUS    RESTARTS   AGE
pod/cpuminer-0        1/1     Running   0          34s
pod/explorer-0        3/3     Running   0          33s
pod/paicoin-node1-0   1/1     Running   0          34s
pod/paicoin-node2-0   1/1     Running   0          34s

NAME                       TYPE        CLUSTER-IP      EXTERNAL-IP   PORT(S)               AGE
service/cpuminer-service   ClusterIP   None            <none>        <none>                34s
service/explorer-service   NodePort    10.109.5.77     <none>        8079:32543/TCP        34s
service/kubernetes         ClusterIP   10.96.0.1       <none>        443/TCP               26d
service/paicoin-service1   ClusterIP   10.98.102.106   <none>        18566/TCP,18567/TCP   34s
service/paicoin-service2   ClusterIP   10.100.131.17   <none>        18566/TCP,18567/TCP   34s

NAME                             READY   AGE
statefulset.apps/cpuminer        1/1     34s
statefulset.apps/explorer        1/1     34s
statefulset.apps/paicoin-node1   1/1     34s
statefulset.apps/paicoin-node2   1/1     34s
```

**IMPORTANT NOTE**: To be able to mine past the stake validation height we need to import the *private key* of the *address* that cpuminer uses as coinbase address.
That is so that *autobuyer* can access the funds needed to buy tickets.

That means that we need to execute an import command on the paicoin-node2-0 pod:
```
$ kubectl exec -it paicoin-node2-0 -- /bin/bash
root@paicoin-node2-0:/# paicoin-cli importprivkey aTHcf6yqGwRE4ntF8Hvy2vgCnzDKnBnHFiRCHEniQYUYfWNPx9XZ
root@paicoin-node2-0:/#
```

Datadir is persisted on the minukube:
```
$ minikube ssh
docker@minikube:~$ ls -la /datadir/
total 16
drwxr-xr-x 4 root root 4096 Aug 19 15:39 .
drwxr-xr-x 1 root root 4096 Aug 19 13:33 ..
drwxr-xr-x 3 root root 4096 Aug 19 15:39 paicoin-node1
drwxr-xr-x 3 root root 4096 Aug 19 15:39 paicoin-node2
```

When you want to start the chain from scratch just delete these folders and **do not forget to re-import the private key** as explained above.


To follow the blockchain progress, we first need to get the url of the explorer-service:
```
$ minikube service list 
|-------------|------------------|--------------|-------------------------|
|  NAMESPACE  |       NAME       | TARGET PORT  |           URL           |
|-------------|------------------|--------------|-------------------------|
| default     | cpuminer-service | No node port |
| default     | explorer-service |         8079 | http://172.17.0.2:32543 |
| default     | kubernetes       | No node port |
| default     | paicoin-service1 | No node port |
| default     | paicoin-service2 | No node port |
| kube-system | kube-dns         | No node port |
|-------------|------------------|--------------|-------------------------|
```
You should get a similar url. Point your browser to it.

Note: To delete the deployment, you should run:
```
$ kubectl delete -f paicoind-deploy.yaml
```
NOTE: by deleting the nodes the blocks are lost, and when re-deployed the chain starts from genesis. 

## Logs and debug

You can view the logs on any of the containers, for example:
```
$ kubectl logs paicoin-node1-0
$ kubectl logs cpuminer-0
$ kubectl logs explorer-0 -c explorer
$ kubectl logs explorer-0 -c mongo
```

To debug you may run commands on containers: 
```
$ kubectl exec -it paicoin-node1-0 -- /bin/bash
$ kubectl exec -it explorer-0 -c explorer -- /bin/sh
```
Note: not all containers have 'bash', but 'sh' should work.
