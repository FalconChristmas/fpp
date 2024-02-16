# Steps for building FPP Docker on Windows
# Install latest PowerShell https://github.com/PowerShell/PowerShell/releases
# Install WSL https://learn.microsoft.com/en-us/windows/wsl/install
#   Open PowerShell
#   wsl --install
# Install Docker Desktop https://docs.docker.com/desktop/install/windows-install/
#   Use 4.25.2 as of 2/15/2024, since a bug in 4.26.0 causes "ERROR: failed to solve: Canceled: context canceled"
#   Tracking issue is https://github.com/docker/for-win/issues/13812
# Set git to handle line endings so that scripts will work
#   git config --global core.autocrlf input
# Pull down FPP repo for git
# Run this powershell script from your local repo directory

# If you already have the FPP repo pulled down, follow these steps to fix the line endings
# Fix line endings - https://docs.github.com/en/get-started/getting-started-with-git/configuring-git-to-handle-line-endings
#  git config --global core.autocrlf input
#  git rm -rf --cached .
#  git reset --hard HEAD

$env:FPPBRANCH = $env:FPPBRANCH ?? "master"

# build the docker image
docker build -t falconchristmas/fpp:latest -f Docker/Dockerfile --build-arg FPPBRANCH=$env:FPPBRANCH .
#or
#docker build -t falconchristmas/fpp:latest  --build-arg EXTRA_INSTALL_FLAG=--skip-clone -f Docker/Dockerfile .

# docker run --name fpp --hostname fppDocker -t -i -p 80:80 -p 4048:4048/udp -p 5568:5568/udp -p 32320:32320/udp -v ${PWD}:/opt/fpp${DOCKER_MOUNT_FLAG} falconchristmas/fpp:latest

# A sample docker-compose.yml file to use the docker image is also
# included in the Docker directory.