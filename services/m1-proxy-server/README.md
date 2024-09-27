# M1 Proxy Server

## Deploying to AWS EC2
### Create an EC2 Instance
- Select AMI: Use the latest Amazon Linux 2 AMI.
- Instance Type: Choose a t2.micro for testing (within Free Tier).
- Security Group:
	- Allow inbound traffic on port 80 (HTTP) or the port you plan to use.
	- Allow SSH access from your IP.

### Connect to the EC2 Instance
Use SSH to connect:

```
ssh -i your-key.pem ec2-user@your-ec2-ip-address
```

### Install Node.js on EC2
On the EC2 instance:

```
curl -sL https://rpm.nodesource.com/setup_14.x | sudo bash -
sudo yum install -y nodejs
```

### Transfer Your Server Code
From your local machine:

```
scp -i your-key.pem -r . ec2-user@your-ec2-ip-address:/home/ec2-user/mixpanel-proxy
````

### Install Dependencies on EC2
On the EC2 instance:

```
cd mixpanel-proxy
npm install
```

### Run the Server
You can use a process manager like pm2 to run the server in the background:

```
sudo npm install -g pm2
pm2 start app.js
```

### Configure Security Group for Your Port
Ensure that the port you're using (default is 3000) is open in your EC2 Security Group.
- Go to the AWS Console.
- Navigate to EC2 > Security Groups.
- Edit the inbound rules to allow traffic on port 3000 (or change the server to run on port 80).

### Update the Server Port (Optional)
To run on port 80, update app.js:

```
const PORT = process.env.PORT || 80;
```

And in app.js:
```
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Mixpanel proxy server is running on port ${PORT}`);
});
```
Note: Ports below 1024 require root privileges. Consider using a reverse proxy like Nginx or running the server on a higher port.

### Set Up a Reverse Proxy with Nginx (Recommended)
Install Nginx:

```
sudo amazon-linux-extras install nginx1
sudo systemctl start nginx
sudo systemctl enable nginx
```
Configure Nginx to proxy requests to your Node.js server.