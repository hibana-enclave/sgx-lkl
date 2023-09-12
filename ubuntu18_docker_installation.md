# Ubuntu Docker Installation 

## Install Docker 

<https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-on-ubuntu-18-04>

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

<https://askubuntu.com/questions/477551/how-can-i-use-docker-without-sudo/1293601#1293601>

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
