Setting Up Gitian on macOS
===========================

Now that [Docker for Mac](./docker-setup-mac.md) has been installed and configured properly, you're ready to do the initial setup of the Gitian build system.

The following steps will be performed using the terminal app, which can be accessed from the Applications/Utilities folder.

<!-- markdown-toc start -->
**Table of Contents**

- [Installing Homebrew](#installing-homebrew)
- [Required system packages](#required-system-packages)
    - [Configuring Git](#configuring-git)
- [Fetching the Build Script](#fetching-the-build-script)
- [Initial Gitian Setup](#initial-gitian-setup)

<!-- markdown-toc end -->

Installing Homebrew
--------------------

[Homebrew](https://brew.sh/) is a macOS package manager (similar to apt or yum) that provides a plethora of libraries and software packages. We use it to provide compatibility with the gitian build process.

Installing [Homebrew](https://brew.sh/) is as easy as running the following command in the terminal app:

```bash
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
This process will show a brief overview of what is going to be installed, and ask you if you wish to continue by pressing ENTER. After confirming that you wish to continue, you will be prompted to enter your password (macOS account password) to proceed.

Required system packages
-------------------------

Python3 and Git are the two base requirements that need to be met for our build setup. Install them with the following command:

```bash
brew install python3 git
```

##### Configuring Git

Once Git is installed, you will need to do some basic configuration to set your name and email. In the below command examples, it is good practice to use your GitHub username and email address:

```bash
git config --global user.name "GITHUB_USERNAME"
git config --global user.email "MY_NAME@example.com"
```

Fetching the Build Script
--------------------------

The build script we'll be using is contained in the Agenor github repository ([contrib/gitian-build.py](https://github.com/AgenorCore/Agenor/blob/master/contrib/gitian-build.py)). Since this is a completely fresh environment, we haven't yet cloned the Agenor repository and will need to fetch this script with the following commands:

```bash
curl -L -O https://raw.githubusercontent.com/PIVX-Project/PIVX/master/contrib/gitian-build.py
chmod +x gitian-build.py
```

This will place the script in your home directory and make it executable.

*Note: Changes to the script in the repository won't be automatically applied after fetching with the above commands. It is good practice to periodically re-run the above commands to ensure your version of the script is always up to date.*

Initial Gitian Setup
-------------------------

Now that the script has been downloaded to your home directory, its time to run it in setup mode. This will perform the following actions:

- Install the necessary system packages for gitian (namely the Docker cli tools).
- Clone the gitian-builder, gitian.sigs, agenor-detached-sigs, and agenor GitHub repos.
- Configure proper user/group permissions for running gitian with Docker
- Create a base Docker image.

Run the following command:

```bash
./gitian-build.py --docker --setup
```
*The `--docker` option instructs the script to use Docker, and the `--setup` option instructs the script to run the setup procedure.*
