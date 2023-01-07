# Ubuntu Docker Installation 

## Install Docker 

```sh
# 
sudo apt update
sudo apt install apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable"

sudo apt update
# apt-cache policy docker-ce
sudo apt install docker-ce
sudo systemctl status docker
```

## Docker without Root Permission 

```sh
sudo groupadd docker
sudo usermod -aG docker $USER
sudo chown root:docker /var/run/docker.sock
sudo chown -R root:docker /var/run/docker
```

Now reboot the machine. 

## Testing 

```sh
docker run hello-world
```
