# docker-user-init

This is meant as a supplement to https://github.com/Yelp/dumb-init to facilitate local development 
without rebuilding containers solely to change the user to the local user 

You can now specify the user with `--user $(id -u):$(id -g)` 
and when the container starts, the id of the default user and group (as specified with USER xxx:xxx in your Dockerfile)  
will be changed to your local user id / group id 

This has the advantage that all files inside the container which are written to in the container (eg: log files) 
will be owned by your local user. This is especially helpful for files on docker volumes

When the container is started as root, nothing will happen, the user ids / group ids in the container remain untouched.
When docker-user-init is done, it will replace itself with dumb-init 

# Example usage

See https://github.com/thisisdevelopment/php 

# Based on 

https://github.com/ncopa/su-exec/
