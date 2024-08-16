.PHONY: up
up:
		@docker compose -f ./docker/docker-compose.yml up -d
.PHONY: down
down:
		@docker compose -f ./docker/docker-compose.yml down
.PHONY: down-all
down-all:
		@docker compose -f ./docker/docker-compose.yml down --rmi all --volumes
