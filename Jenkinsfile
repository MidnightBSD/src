
pipeline {
    agent any

    stages {
        stage('Prepare') {
            steps {
                sh 'make clean' 
            }
        }
        stage('Build') {
            steps {
                sh 'make tinderbox' 
            }
        }
    }
}
