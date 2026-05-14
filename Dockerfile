# Use an official Ubuntu runtime as a base image
FROM ubuntu:latest

# Set the working directory inside the container
WORKDIR /app

# Install build-essential (includes gcc/g++) and valgrind
RUN apt-get update && apt-get install -y build-essential valgrind gdb

# Command to run an interactive bash shell when the container starts
CMD ["/bin/bash"]
