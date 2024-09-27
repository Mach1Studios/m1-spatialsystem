# Makefile

.PHONY: install start test deploy clean

install:
	npm install

start:
	node app.js

test:
	@echo "Starting local server..."
	@node app.js &
	@sleep 1
	@echo "Sending test event..."
	@curl -X POST -H "Content-Type: application/json" -d '{"event": "Test Event", "properties": {"distinct_id": "test_user"}}' http://localhost:3000/track
	@echo "\nTest completed."

deploy:
	@echo "Deploying to AWS EC2..."
	# Add your deployment commands here (e.g., SCP files to EC2 instance)
	# For example:
	# scp -i your-key.pem -r . ec2-user@your-ec2-ip-address:/home/ec2-user/mixpanel-proxy
	# ssh -i your-key.pem ec2-user@your-ec2-ip-address 'cd mixpanel-proxy && npm install && pm2 restart app.js'

clean:
	rm -rf node_modules/
