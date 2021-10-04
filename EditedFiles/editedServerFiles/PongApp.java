/*
 * The MIT License (MIT)
 *
 * FXGL - JavaFX Game Library
 *
 * Copyright (c) 2015-2017 AlmasB (almaslvl@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package com.almasb.fxglgames.pong;

import com.almasb.fxgl.animation.Interpolators;
import com.almasb.fxgl.app.ApplicationMode;
import com.almasb.fxgl.app.GameApplication;
import com.almasb.fxgl.app.GameSettings;
import com.almasb.fxgl.core.math.FXGLMath;
import com.almasb.fxgl.entity.Entity;
import com.almasb.fxgl.entity.SpawnData;
import com.almasb.fxgl.input.UserAction;
import com.almasb.fxgl.net.*;
import com.almasb.fxgl.physics.CollisionHandler;
import com.almasb.fxgl.physics.HitBox;
import com.almasb.fxgl.ui.UI;

import javafx.scene.input.KeyCode;
import javafx.scene.paint.Color;
import javafx.util.Duration;

import java.io.*;

import java.util.*;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

import static com.almasb.fxgl.dsl.FXGL.*;
import static com.almasb.fxglgames.pong.NetworkMessages.*;

/**
 * A simple clone of Pong.
 * Sounds from https://freesound.org/people/NoiseCollector/sounds/4391/ under CC BY 3.0.
 *
 * @author Almas Baimagambetov (AlmasB) (almaslvl@gmail.com)
 */
public class PongApp extends GameApplication implements MessageHandler<String> {

    @Override
    protected void initSettings(GameSettings settings) {
        settings.setTitle("Pong");
        settings.setVersion("1.0");
        settings.setFontUI("pong.ttf");
        settings.setApplicationMode(ApplicationMode.DEBUG);
    }

    private Entity player1; //Entity information for player1
    private Entity player2; //Entity information for player2
    private Entity ball; //Entity information for ball
    private BatComponent player1Bat;
    private BatComponent player2Bat;
    private BallComponent ballCom;

    private boolean countInit = false; //has the countdown been initiated

    private Server<String> server;
    private Connection player1Conn, player2Conn; //Connections that have been assigned player1 or player2
    private ArrayList<Connection> activeConnList = new ArrayList<>(); //The connections that are still active
    private BlockingQueue<Connection> confirmed = new ArrayBlockingQueue<>(25); //The connections that have confirmed they are still active

    /**
     * Takes the connections that are currently registered as connected and sends a message to them.
     * After 5 seconds, any connections that have not sent the expected reply are terminated. A Blocking
     * Queue is used for the confirmed connections as access is shared between Threads.
     */
    Thread activeCheck = new Thread() {
        private ArrayList<Connection> terminationPending; //The connections that have not yet confirmed

        public void run() {
            while (true) {
                terminationPending = new ArrayList<>(activeConnList);

                for (Connection c : terminationPending) {
                    if (c.isConnected()) {
                        c.send("CONN_CHECK");
                    }
                }
                try {
                    this.sleep(5000); //gives client time to send reply
                    while (confirmed.peek() != null) {
                        Connection head = confirmed.take();
                        terminationPending.removeIf(n -> n == head);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }

                for (Connection c : terminationPending) {
                    if (c.isConnected()) {
                        System.out.println("No reply from connection: " + c.getConnectionNum() + ". Terminating.");
                        c.terminate();
                    }
                }
            }
        }
    };

    /**
     * Initialises the input events for keys W, S, I and K.
     * Changes made: N/A
     */
    @Override
    protected void initInput() {
        getInput().addAction(new UserAction("Up1") {
            @Override
            protected void onAction() {
                player1Bat.up();
            }

            @Override
            protected void onActionEnd() {
                player1Bat.stop();
            }
        }, KeyCode.W);

        getInput().addAction(new UserAction("Down1") {
            @Override
            protected void onAction() {
                player1Bat.down();
            }

            @Override
            protected void onActionEnd() {
                player1Bat.stop();
            }
        }, KeyCode.S);

        getInput().addAction(new UserAction("Up2") {
            @Override
            protected void onAction() {
                player2Bat.up();
            }

            @Override
            protected void onActionEnd() {
                player2Bat.stop();
            }
        }, KeyCode.I);

        getInput().addAction(new UserAction("Down2") {
            @Override
            protected void onAction() {
                player2Bat.down();
            }

            @Override
            protected void onActionEnd() {
                player2Bat.stop();
            }
        }, KeyCode.K);
    }

    /**
     * Initialises the score variables.
     * Changes made: N/A
     * @param vars map to put variables into.
     */
    @Override
    protected void initGameVars(Map<String, Object> vars) {
        vars.put("player1score", 0);
        vars.put("player2score", 0);
    }

    /**
     * Initialises the game.
     * Changes made: Sets the event called for when a new connection is made which assigns a Role by
     * sending the relevant message. Starts the active connection check thread.
     */
    @Override
    protected void initGame() {
        Writers.INSTANCE.addTCPWriter(String.class, outputStream -> new MessageWriterS(outputStream));
        Readers.INSTANCE.addTCPReader(String.class, in -> new MessageReaderS(in));

        server = getNetService().newTCPServer(55555, new ServerConfig<>(String.class));

        server.setOnConnected(connection -> {
            connection.addMessageHandlerFX(this);
            activeConnList.add(connection);
            System.out.println("NEW CONNECTION");

            //Assign player to connection
            if (player1Conn == null || !player1Conn.isConnected()) {
                player1Conn = connection;
                connection.send("ROLE,1");
            } else if (player2Conn == null || !player2Conn.isConnected()) {
                player2Conn = connection;
                connection.send("ROLE,2");
            } else {
                connection.send("ROLE,3");
            }
        });


        getGameWorld().addEntityFactory(new PongFactory());
        getGameScene().setBackgroundColor(Color.rgb(0, 0, 5));

        initScreenBounds();
        initGameObjects();

        var t = new Thread(server.startTask()::run);
        t.setDaemon(true);
        t.start();

        //run background thread to check for connection timeout on currently active connections.
        activeCheck.setDaemon(true);
        activeCheck.start();
    }

    /**
     * Initialises the physics variables for the game including collision events.
     * Changes made: Minimised the unique calls to send a message.
     */
    @Override
    protected void initPhysics() {
        getPhysicsWorld().setGravity(0, 0);

        getPhysicsWorld().addCollisionHandler(new CollisionHandler(EntityType.BALL, EntityType.WALL) {
            @Override
            protected void onHitBoxTrigger(Entity a, Entity b, HitBox boxA, HitBox boxB) {

                if (boxB.getName().equals("LEFT")) {
                    inc("player2score", +1);
                    sendToActive(HIT_WALL_LEFT + "," + geti("player1score") + "," + geti("player2score"));
                } else if (boxB.getName().equals("RIGHT")) {
                    inc("player1score", +1);
                    sendToActive(HIT_WALL_RIGHT + "," + geti("player1score") + "," + geti("player2score"));
                } else if (boxB.getName().equals("TOP")) {
                    sendToActive(HIT_WALL_UP);
                } else if (boxB.getName().equals("BOT")) {
                    sendToActive(HIT_WALL_DOWN);
                }

                getGameScene().getViewport().shakeTranslational(5);
            }
        });

        CollisionHandler ballBatHandler = new CollisionHandler(EntityType.BALL, EntityType.PLAYER_BAT) {
            @Override
            protected void onCollisionBegin(Entity a, Entity bat) {
                playHitAnimation(bat);

                sendToActive(bat == player1 ? BALL_HIT_BAT1 : BALL_HIT_BAT2);
            }
        };

        getPhysicsWorld().addCollisionHandler(ballBatHandler);
        getPhysicsWorld().addCollisionHandler(ballBatHandler.copyFor(EntityType.BALL, EntityType.ENEMY_BAT));
    }

    /**
     * Initialises the UI variables.
     * Changes made: N/A
     */
    @Override
    protected void initUI() {
        MainUIController controller = new MainUIController();
        UI ui = getAssetLoader().loadUI("main.fxml", controller);

        controller.getLabelScorePlayer().textProperty().bind(getip("player1score").asString());
        controller.getLabelScoreEnemy().textProperty().bind(getip("player2score").asString());

        getGameScene().addUI(ui);
    }

    /**
     * Game Logic update loop.
     * Changes made: Remove any terminated connections from the 'active' array. Append relevant win
     * message to information sent to Client. Initiate counter if not already & ball velocity 0 and
     * both players present. Pause game if both players are not present and reset game if win met.
     * @param tpf
     */
    @Override
    protected void onUpdate(double tpf) {
        //if any connections have ever been present, else pause ball.
        if (!server.getConnections().isEmpty()) {
            activeConnList.removeIf(n -> !n.isConnected());

            var message = "GAME_DATA," + player1.getY() + "," + player2.getY() + "," + ball.getX() + "," + ball.getY();
            if (geti("player1score") == 10) {
                message += ",1WIN,";
            } else if (geti("player2score") == 10) {
                message += ",2WIN,";
            } else {
                message += ",0WIN,";
            }

            sendToActive(message); //replaces server.broadcast(message);

            //If both players assigned
            if (player1Conn != null && player2Conn != null) {
                //if both players connected and ball is stationary, do counter. else pause ball.
                if ((!ballCom.isMoving()) && (player1Conn.isConnected() && player2Conn.isConnected())) {
                    if (!countInit) {
                        Timer counter = new Timer();
                        counter.schedule(new TimerTask() {
                            @Override
                            public void run() {
                                sendToActive("COUNT,0");
                                ballCom.restart();
                            }
                        }, 3000);
                        counter.schedule(new countdownTask(3), 0);
                        counter.schedule(new countdownTask(2), 1000);
                        counter.schedule(new countdownTask(1), 2000);
                        countInit = true;
                    }
                } else if (!player1Conn.isConnected() || !player2Conn.isConnected()) {
                    stopBall();
                }

                //if win condition met, reset the game.
                if (geti("player1score") == 10 || geti("player2score") == 10) {
                    ball.removeFromWorld();
                    player1.removeFromWorld();
                    player2.removeFromWorld();
                    initGameObjects();
                    set("player1score", 0);
                    set("player2score", 0);
                    stopBall();
                }
            }
        } else {
            stopBall();
        }

    }

    /**
     * Send commands and args only to connections that are active.
     * @param message The message to be sent to connections.
     */
    private synchronized void sendToActive(String message) {
        for (Connection c : activeConnList) {
            c.send(message);
        }
    }

    /**
     * Set Ball Velocity to 0 and reset Counter initialisation.
     */
    private void stopBall() {
        ballCom.stop();
        countInit = false;
    }

    /**
     * Build collision for world bounds.
     * Changes made: N/A
     */
    private void initScreenBounds() {
        Entity walls = entityBuilder()
                .type(EntityType.WALL)
                .collidable()
                .buildScreenBounds(150);

        getGameWorld().addEntity(walls);
    }

    /**
     * Initialise the game objects and entities.
     * Changes made: N/A
     */
    private void initGameObjects() {
        ball = spawn("ball", getAppWidth() / 2 - 5, getAppHeight() / 2 - 5);
        player1 = spawn("bat", new SpawnData(getAppWidth() / 4, getAppHeight() / 2 - 30).put("isPlayer", true));
        player2 = spawn("bat", new SpawnData(3 * getAppWidth() / 4 - 20, getAppHeight() / 2 - 30).put("isPlayer", false));

        player1Bat = player1.getComponent(BatComponent.class);
        player2Bat = player2.getComponent(BatComponent.class);
        ballCom = ball.getComponent(BallComponent.class);
    }

    /**
     * Called to play animation on bat hit using FXGL.
     * @param bat The entity to apply the animation to.
     */
    private void playHitAnimation(Entity bat) {
        animationBuilder()
                .autoReverse(true)
                .duration(Duration.seconds(0.5))
                .interpolator(Interpolators.BOUNCE.EASE_OUT())
                .rotate(bat)
                .from(FXGLMath.random(-25, 25))
                .to(0)
                .buildAndPlay();
    }

    /**
     * Handle messages sent by Client.
     * Changes made: Check connection control and mock key presses accordingly. Terminate connection if
     * informed that it is exiting. Reconfigure connection-entity association for control take over.
     * Inform timeoutCheck that connection is confirmed via the confirmed BlockingQueue.
     * @param connection The connection that has sent the message.
     * @param message The message that has been sent.
     */
    @Override
    public void onReceive(Connection<String> connection, String message) {
        KeyCode key;
        var tokens = message.split(","); //separation of message

        key = KeyCode.valueOf(tokens[1].substring(0, 1));

        if (connection.getConnectionNum() == player1Conn.getConnectionNum()) {
            //we don't want to switch the keys to mock on server in this situation
        } else if (connection.getConnectionNum() == player2Conn.getConnectionNum()) {
            if (key == KeyCode.W) {
                key = KeyCode.I;
            }
            if (key == KeyCode.S) {
                key = KeyCode.K;
            }
        } else {
            key = KeyCode.Q; //arbitrary key so spectators can't control a player
        }

        if (tokens[1].endsWith("_DOWN")) {
            getInput().mockKeyPress(key);
        } else if (tokens[1].endsWith("_UP")) {
            getInput().mockKeyRelease(key);
        }

        if (tokens[1].equals("CON_CLOSE")) {
            System.out.println("Terminating:");
            connection.terminate();
            if (connection == player1Conn) {
                System.out.println("Player1 Free");
            }
            if (connection == player2Conn) {
                System.out.println("Player2 Free");
            }
        }

        if (tokens[1].equals("TAKE_OVER")) {
            if (!player1Conn.isConnected()) {
                player1Conn = connection;
                connection.send("ROLE,1");
            } else if (!player2Conn.isConnected()) {
                player2Conn = connection;
                connection.send("ROLE,2");
            }
        }

        if (tokens[1].equals("CONFIRM")) {
            try {
                confirmed.put(connection);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Time delayed code to act as countdown for game start, informs connections.
     */
    class countdownTask extends TimerTask {
        private final int seconds;

        countdownTask (int seconds) {
            this.seconds = seconds;
        }
        public void run() {
            sendToActive("COUNT," + seconds);
        }
    }

    /**
     * Handles outgoing messages.
     * Changes made: N/A
     */
    static class MessageWriterS implements TCPMessageWriter<String> {

        private OutputStream os;
        private PrintWriter out;

        MessageWriterS(OutputStream os) {
            this.os = os;
            out = new PrintWriter(os, true);
        }

        @Override
        public void write(String s) throws Exception {
            out.print(s.toCharArray());
            out.flush();
        }
    }

    /**
     * Handles incoming messages.
     * Changes made: N/A
     */
    static class MessageReaderS implements TCPMessageReader<String> {

        private BlockingQueue<String> messages = new ArrayBlockingQueue<>(50);

        private InputStreamReader in;

        MessageReaderS(InputStream is) {
            in =  new InputStreamReader(is);
            var t = new Thread(() -> {
                try {

                    char[] buf = new char[36];

                    int len;

                    while ((len = in.read(buf)) > 0) {
                        var message = new String(Arrays.copyOf(buf, len));

                        System.out.println("Recv message: " + message);

                        messages.put(message);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            });

            t.setDaemon(true);
            t.start();
        }

        @Override
        public String read() throws Exception {
            return messages.take();
        }
    }

    public static void main(String[] args) {
        launch(args);
    }
}