.PHONY: d-build d-run d-destruct

d-build:
		docker build -t netgo ./docker

d-run:
		docker run -t -d --name netgo \
			--cap-add=NET_ADMIN \
			--log-opt max-size=10m \
			--log-opt max-file=5 \
			-v ./:/home/tcp \
			-p 8080:8080 \
			netgo

d-destruct:
		docker stop netgo
		docker rm netgo
		docker rmi netgo
