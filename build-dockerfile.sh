#!/bin/bash

set -e  # Exit on any error

# Help function
show_help() {
    echo "🔧 3D Tiles Docker Build Script"
    echo "📖 Usage: $0 [user] [branch]"
    echo ""
    echo "🎯 Examples:"
    echo "  $0                    # Build fanvanzh/3dtiles master branch"
    echo "  $0 dev                # Build fanvanzh/3dtiles dev branch"
    echo "  $0 myuser mybranch    # Build myuser/3dtiles mybranch branch"
    echo ""
    echo "📦 Output Docker images:"
    echo "  - No args:    3dtiles:master-latest"
    echo "  - 1 arg:      3dtiles:{branch}-latest"
    echo "  - 2 args:     3dtiles:{user}-{branch}-latest"
}

# Check for help flags
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    show_help
    exit 0
fi

if [ $# == 0 ]; then
    echo "🚀 Building fanvanzh/3dtiles:master branch..."
    docker build -t 3dtiles:master-latest --platform linux/amd64 --build-arg REPO_REF=master .
    echo "✅ Built image: 3dtiles:master-latest"
elif [ $# == 1 ]; then
    echo "🚀 Building fanvanzh/3dtiles:${1} branch..."
    docker build -t "3dtiles:${1}-latest" --platform linux/amd64 --build-arg REPO_REF=$1 .
    echo "✅ Built image: 3dtiles:${1}-latest"
elif [ $# == 2 ]; then
    echo "🚀 Building ${1}/3dtiles:${2} branch..."
    docker build -t "3dtiles:${1}-${2}-latest" --platform linux/amd64 --build-arg REPO_USER=$1 --build-arg REPO_REF=$2 .
    echo "✅ Built image: 3dtiles:${1}-${2}-latest"
else
    echo "❌ Too many arguments!"
    show_help
    exit 1
fi

