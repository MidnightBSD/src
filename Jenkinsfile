pipeline {
    parameters {
        choice(name: 'ARCHITECTURE_FILTER', choices: ['all', 'amd64', 'i386'], description: 'Run on specific architecture')
    }
    agent none
    stages {
        stage('Check Branch Name') {
            steps {
                script {
                    if (env.BRANCH_NAME.startsWith('vendor/')) {
                        currentBuild.result = 'NOT_BUILT'
                        error('Skipping build for vendor branch')
                    }
                }
            }
        }
        stage('BuildAndTest') {
            matrix {
                agent {
                    label "${ARCHITECTURE} && bsd"
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
                            catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
                                echo "Do buildworld for ${ARCHITECTURE}"
                                sh 'make -j5 buildworld'
                            }
                        }
                    }
                    stage('buildkernel') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                        }
                        steps {
                            echo "Do buildkernel for ${ARCHITECTURE}"
                             sh 'make -j5 buildkernel' 
                        }
                    }
                    stage('tests') {
                        environment {
                            KYUA_RESULTS = "${env.WORKSPACE}/kyua-results-${ARCHITECTURE}.db"
                            JUNIT_RESULTS = "junit-results-${ARCHITECTURE}.xml"
                        }
                        steps {
                            echo "Do tests for ${ARCHITECTURE}"
                            sh "rm -f ${KYUA_RESULTS} ${JUNIT_RESULTS}"
                            sh "kyua test -k tests/Kyuafile --results-file ${KYUA_RESULTS}"
                            sh "kyua report-junit --output ${JUNIT_RESULTS} --results-file ${KYUA_RESULTS}"
                        }
                        post {
                            always {
                                junit allowEmptyResults: true, testResults: "${JUNIT_RESULTS}"
                            }
                        }
                    }
                }
            }
        }
    }
}
