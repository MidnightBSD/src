
pipeline {
    agent any
    
    // Throttle a declarative pipeline via options
    options {
      throttleJobProperty(
          categories: ['bsd'],
          throttleEnabled: true,
          throttleOption: 'category'
      )
    }

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
