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
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                        }
                        steps {
                            echo "Prepare for ${ARCHITECTURE}"
                            sh "mkdir -p ${MAKEOBJDIRPREFIX}"
                            sh 'make clean' 
                        }
                    }
                    stage('buildworld') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                        }
                        steps {
                            echo "Do buildworld for ${ARCHITECTURE}"
                             sh 'make buildworld'
                        }
                    }
                    stage('buildkernel') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                        }
                        steps {
                            echo "Do buildkernel for ${ARCHITECTURE}"
                             sh 'make buildkernel' 
                        }
                    }
                    stage('tests') {
                        steps {
                            echo "Do tests for ${ARCHITECTURE}"
                            sh 'kyua test -k tests/Kyuafile' 
                        }
                    }
                }
            }
        }
    }
}
