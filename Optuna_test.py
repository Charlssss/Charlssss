import pandas as pd
import numpy as np
import lightgbm as lgb
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, confusion_matrix, classification_report
from sklearn.datasets import make_classification
import optuna

X, y = make_classification(
    n_samples=60,
    n_features=6,
    n_informative=4,
    n_redundant=1,
    n_classes=2,
    random_state=42,
    weights=[0.667, 0.333]
)
feature_names = [f'feature_{i}' for i in range(X.shape[1])]
X_df = pd.DataFrame(X, columns=feature_names)
y_s = pd.Series(y, name='target')

X_train, X_val, y_train, y_val = train_test_split(
    X_df, y_s, test_size=10, stratify=y_s, random_state=42
)

def objective(trial):
    params = {
        'objective': 'binary',
        'metric': 'binary_logloss',
        'n_jobs': -1,
        'random_state': 42,
        'is_unbalanced': False,
        'verbose': -1,

        'n_estimators':trial.suggest_int('n_estimators', 10, 1000),
        'learning_rate': trial.suggest_float('learning_rate', 0.001, 0.5, log=True),
        'num_leaves': trial.suggest_int('num_leaves', 10, 100),
        'min_child_samples': trial.suggest_int('min_child_samples', 10, 100),
        'reg_alpha': trial.suggest_float('reg_alpha', 0.001, 2.0, log=True),
        'reg_lambda': trial.suggest_float('reg_lambda', 0.001, 2.0, log=True),
        'max_depth': trial.suggest_int('max_depth', 1, 10),
        'scale_pos_weight': trial.suggest_float('scale_pos_weight', 1.0, 5.0),
    }

    model = lgb.LGBMClassifier(**params)

    pruning_callback = optuna.integration.LightGBMPruningCallback(
        trial,
        'binary_logloss'
    )

    model.fit(
        X_train, y_train,
        eval_set=[(X_val, y_val)],
        eval_metric='binary_logloss',
        callbacks=[
            lgb.early_stopping(100, verbose=False),
            pruning_callback
        ]
    )

    best_loss = model.best_score_['valid_0'][params['metric']]

    return best_loss

print("\nStarting Bayesian Optimization...")
optuna.logging.set_verbosity(optuna.logging.WARNING)

study = optuna.create_study(
    direction='minimize',
    pruner=optuna.pruners.MedianPruner()
)
study.optimize(objective, n_trials=100) # 100 trials is the most optimal

print("\nOptimization Finished.")
print(f"Best trial validation loss: {study.best_value:.4f}")
print("Best hyperparameters found:")
print(study.best_params)

print("\nSaving optimization results to HTML files...")
try:
    fig_history = optuna.visualization.plot_optimization_history(study)
    fig_history.write_html("optimization_history.html")

    fig_slice = optuna.visualization.plot_slice(study)
    fig_slice.write_html("slice_plot.html")

    print("\nPlots saved! Check your script's folder.")
except (ImportError, ModuleNotFoundError):
    print("\nCould not save plots. Please run 'pip install plotly'.")
except Exception as e:
    print(f"\nCould not save plots due to an error: {e}")

print("\nTraining final model with best hyperparameters...")
best_params = study.best_params
best_params['objective'] = 'binary'
best_params['metric'] = 'binary_logloss'
best_params['n_jobs'] = -1
best_params['random_state'] = 42
best_params['is_unbalanced'] = False

final_model = lgb.LGBMClassifier(**best_params)
final_model.fit(
    X_train, y_train,
    eval_set=[(X_val, y_val)],
    eval_metric='binary_logloss',
    callbacks=[
        lgb.early_stopping(100),
        lgb.log_evaluation(100)]
)

print("\nEvaluating final model...")
y_pred_class = final_model.predict(X_val)
accuracy = accuracy_score(y_val, y_pred_class)

print(f"\nBest Iteration: {final_model.best_iteration_}")
print(f"Final Validation Accuracy: {accuracy:.4f}")

print("\nConfusion Matrix:")
# confusion matrix
#
#       Predicted 0   Predicted 1
# Is 0   [TN]          [FP]
# Is 1   [FN]          [TP]
#
cm = confusion_matrix(y_val, y_pred_class)
print(cm)

print("\nClassification Report:")
print(classification_report(y_val, y_pred_class, target_names=['False (0)', 'True (1)']))


# print("\nPlotting feature importance...")
# lgb.plot_importance(final_model, max_num_features=15)
# plt.title("Feature Importance (Final Model)")
# plt.show()
