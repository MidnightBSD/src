pipeline {
    parameters {
        choice(name: 'ARCHITECTURE_FILTER', choices: ['all', 'amd64', 'i386'], description: 'Run on specific architecture')
    }
    agent none
    options {
        buildDiscarder(logRotator(numToKeepStr: '10', artifactNumToKeepStr: '5'))
    }
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
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Prepare for ${ARCHITECTURE}"
                            sh "mkdir -p ${MAKEOBJDIRPREFIX}"
                            sh "rm -rf ${DESTDIR}"
                            sh "mkdir -p ${DESTDIR}"
                            sh 'make clean'
                        }
                    }
                    stage('buildworld') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
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
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do buildkernel for ${ARCHITECTURE}"
                             sh 'make -j5 buildkernel'
                        }
                    }
                    stage('installworld') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do installworld for ${ARCHITECTURE}"
                            sh 'make -DNO_ROOT -DDB_FROM_SRC installworld DESTDIR=${DESTDIR}'
                            sh 'make -DNO_ROOT -DDB_FROM_SRC distribution DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('installkernel') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do installkernel for ${ARCHITECTURE}"
                            sh 'make -DNO_ROOT -DDB_FROM_SRC installkernel DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('tests-install') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do tests-install for ${ARCHITECTURE}"
                            sh 'make -DNO_ROOT -DDB_FROM_SRC tests-install DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('tests') {
                        environment {
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                            KYUA_RESULTS = "${env.WORKSPACE}/kyua-results-${ARCHITECTURE}.db"
                            JUNIT_RESULTS = "junit-results-${ARCHITECTURE}.xml"
                        }
                        steps {
                            echo "Do tests for ${ARCHITECTURE}"
                            sh "rm -f ${KYUA_RESULTS} ${JUNIT_RESULTS}"
                            sh "kyua test -k ${DESTDIR}/usr/tests/Kyuafile --results-file ${KYUA_RESULTS} || true"
                            sh "kyua report-junit --output ${JUNIT_RESULTS} --results-file ${KYUA_RESULTS}"
                        }
                        post {
                            always {
                                junit allowEmptyResults: true, testResults: "${JUNIT_RESULTS}"
                            }
                        }
                    }
                }
                post {
                    always {
                        sh "rm -rf ${env.WORKSPACE}/destdir/${ARCHITECTURE} ${env.WORKSPACE}/obj"
                    }
                }
            }
        }
    }
}
