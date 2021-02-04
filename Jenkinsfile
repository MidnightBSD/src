pipeline {
    parameters {
        choice(name: 'ARCHITECTURE_FILTER', choices: ['all', 'amd64', 'i386'], description: 'Run on specific architecture')
    }
    agent none
    stages {
        stage('BuildAndTest') {
            matrix {
                agent {
                    label "${ARCHITECTURE}"
                }
                
                when { anyOf {
                    expression { params.ARCHITECTURE_FILTER == 'all' }
                    expression { params.ARCHITECTURE_FILTER == env.PLATFORM }
                } }
                axes {
                    axis {
                        name 'ARCHITECTURE'
                        values 'amd64', 'i386'
                    }
                }
                stages {
                    stage('Prepare') {
                        steps {
                            echo "Prepare for ${ARCHITECTURE}"
                            sh 'make clean' 
                        }
                    }
                    stage('Build') {
                        steps {
                            echo "Do Test for ${ARCHITECTURE}"
                             sh 'make -j4 buildworld'
                             sh 'make -j4 buildkernel'  
                        }
                    }
                }
            }
        }
    }
}
