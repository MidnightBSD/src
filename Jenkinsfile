pipeline {
    parameters {
        choice(name: 'ARCHITECTURE_FILTER', choices: ['all', 'amd64', 'i386'], description: 'Run on specific architecture')
    }
    environment {
       MAKEOBJDIRPREFIX = "${env.WORKSPACE}\\obj"
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
                    expression { params.ARCHITECTURE_FILTER == env.ARCHITECTURE }
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
                    stage('buildworld') {
                        steps {
                            echo "Do buildworld for ${ARCHITECTURE}"
                             sh 'make -j4 buildworld'
                        }
                    }
                     stage('buildkernel') {
                        steps {
                            echo "Do buildkernel for ${ARCHITECTURE}"
                             sh 'make -j4 -DMAKE_JUST_KERNELS universe' 
                        }
                    }
                }
            }
        }
    }
}
